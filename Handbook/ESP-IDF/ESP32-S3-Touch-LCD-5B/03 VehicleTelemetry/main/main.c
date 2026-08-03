/**
 * LVGL speedometer dashboard on the Waveshare ESP32-S3-Touch-LCD-5B 5" LCD
 * (1024x600, ST7262 RGB direct-drive).
 *
 * Pipeline in app_main():
 *   1. I2C bus (GPIO8/GPIO9) for the CH422G I/O expander.
 *   2. CH422G: enable EXIO outputs, reset the LCD + touch, keep backlight OFF.
 *   3. RGB LCD panel (esp_lcd) at 1024x600 RGB565, frame buffer in PSRAM.
 *   4. esp_lvgl_port: spin up LVGL (own task + tick + mutex) and bind it to the
 *      RGB panel. LVGL's draw buffers live in PSRAM; on flush, dirty areas are
 *      copied into the panel's PSRAM frame buffer (esp_lcd_panel_draw_bitmap).
 *   5. Build the speedometer UI + start the needle animation (under the LVGL lock).
 *   6. Turn the backlight ON (CH422G) so the image becomes visible.
 *
 * The RGB panel self-refreshes from PSRAM via DMA; LVGL keeps redrawing into it
 * from its own task, so the needle animation runs with no app-side render loop.
 *
 * Tuning notes (if the screen misbehaves - see docs/Display-Bringup-Log.md):
 *   - Garbled / shifted image  -> adjust the RGB porch values in panel_cfg.timings.
 *   - Colours look swapped      -> reorder data_gpio_nums (R/G/B bit order),
 *                                  or LV_COLOR_16_SWAP in sdkconfig.
 *   - Screen stays dark         -> check CH422G I2C / EXIO levels (backlight).
 *   - PCLK must stay <= 16 MHz with fb_in_psram (21 MHz -> blank white screen).
 */
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/i2c_master.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "ch422g.h"
#include "lvgl_dashboard.h"

static const char *TAG = "hello_lcd";

/* ---- I2C (shared bus for CH422G / GT911 / RTC) ---- */
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_FREQ_HZ   100000

/* ---- LCD target resolution ----
 *
 * The 5B ships in two panel variants only: 1024x600 or 800x480. Default is
 * 1024x600 (confirmed by silkscreen, and by the official demo's failure mode:
 * it flashes 800x480 and produces a clean "right ~1/4 white, rest black" band
 * on this panel - which only happens when the *panel* is wider than the
 * *signal*, i.e. panel = 1024).
 *
 * Timing sources (cross-checked against known-working 5B / 7B configs):
 *   1024x600 - ESPHome "WAVESHARE-5-1024X600" + VaAndCob ch422g driver.
 *              PCLK is capped at 16 MHz, NOT the Arduino board-def's 21 MHz:
 *              at 21 MHz the PSRAM->GDMA path underruns, LCD_CAM stops emitting
 *              valid DE, and the ST7262 shows a blank white screen.
 *   800x480  - Waveshare ESP-IDF demo 08_lvgl_Porting (#else branch)
 */
#define LCD_TARGET_RESOLUTION   1024      /* 1024 or 800 */

#if (LCD_TARGET_RESOLUTION == 1024)
  #define LCD_H_RES             1024
  #define LCD_V_RES             600
  #define LCD_PIXEL_CLK_HZ      (16 * 1000 * 1000)
  #define LCD_HSYNC_PULSE       30
  #define LCD_HSYNC_BACK        40
  #define LCD_HSYNC_FRONT       220
  #define LCD_VSYNC_PULSE       4
  #define LCD_VSYNC_BACK        8
  #define LCD_VSYNC_FRONT       4
#else
  #define LCD_H_RES             800
  #define LCD_V_RES             480
  #define LCD_PIXEL_CLK_HZ      (16 * 1000 * 1000)
  #define LCD_HSYNC_PULSE       4
  #define LCD_HSYNC_BACK        8
  #define LCD_HSYNC_FRONT       8
  #define LCD_VSYNC_PULSE       4
  #define LCD_VSYNC_BACK        8
  #define LCD_VSYNC_FRONT       8
#endif

static esp_err_t app_i2c_init(i2c_master_bus_handle_t *out_bus)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&bus_cfg, out_bus);
}

void app_main(void)
{
    ESP_LOGI(TAG, "LVGL speedometer on the Waveshare 5\" 1024x600 LCD");

    /* 1) I2C bus for the CH422G I/O expander. */
    i2c_master_bus_handle_t i2c_bus = NULL;
    ESP_ERROR_CHECK(app_i2c_init(&i2c_bus));

    /* 2) CH422G: enable EXIO outputs, then reset the LCD + touch (backlight off). */
    ESP_LOGI(TAG, "Initialize IO expander (CH422G)");
    ch422g_handle_t io = {0};
    ESP_ERROR_CHECK(ch422g_init(&io, i2c_bus));

    ch422g_set_outputs(&io, 0);                                  /* assert reset, BL off */
    vTaskDelay(pdMS_TO_TICKS(10));
    ch422g_set_outputs(&io, CH422G_LCD_RST | CH422G_TP_RST);     /* release reset, BL off */
    vTaskDelay(pdMS_TO_TICKS(120));                              /* let the panel boot */

    /* 3) Create the RGB LCD panel (frame buffer allocated in PSRAM). */
    ESP_LOGI(TAG, "Install RGB LCD panel driver");
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_rgb_panel_config_t panel_cfg = {
        .data_width = 16,                       /* RGB565 = 16 bits */
        .psram_trans_align = 64,
        .bounce_buffer_size_px = 10 * LCD_H_RES,/* SRAM bounce buffer for PSRAM fb             */
        .num_fbs = 2,                           /* double fb -> esp_lvgl_port can vsync-swap   */
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .disp_gpio_num = -1,                    /* backlight is on the CH422G, not a GPIO */
        .pclk_gpio_num = 7,
        .vsync_gpio_num = 3,
        .hsync_gpio_num = 46,
        .de_gpio_num = 5,
        .data_gpio_nums = {
            /* RGB565 bit order: blue (low) -> green -> red (high), per board pinout. */
            14, 38, 18, 17, 10,        /* B3..B7 */
            39, 0, 45, 48, 47, 21,     /* G2..G7 */
            1, 2, 42, 41, 40,          /* R3..R7 */
        },
        .timings = {
            .pclk_hz = LCD_PIXEL_CLK_HZ,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = LCD_HSYNC_PULSE,
            .hsync_back_porch  = LCD_HSYNC_BACK,
            .hsync_front_porch = LCD_HSYNC_FRONT,
            .vsync_pulse_width = LCD_VSYNC_PULSE,
            .vsync_back_porch  = LCD_VSYNC_BACK,
            .vsync_front_porch = LCD_VSYNC_FRONT,
            .flags.pclk_active_neg = true,
        },
        .flags.fb_in_psram = true,              /* two fbs in PSRAM (~2.3 MB) for tear-free */
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_cfg, &panel));

    /* new_rgb_panel() configures LCD_CAM + GDMA, but call panel_init() to make
     * sure the PSRAM frame-buffer refresh is actually running - otherwise some
     * IDF versions leave the panel emitting no DE and the ST7262 goes blank. */
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    /* 4) esp_lvgl_port: bring up LVGL (own task + tick + mutex) and bind it to
     *    the RGB panel. Tear-free path: the panel owns TWO PSRAM frame buffers
     *    (num_fbs=2 above) and LVGL draws directly into them (direct_mode). On
     *    flush, esp_lvgl_port swaps the active fb at the next bounce-frame
     *    boundary (avoid_tearing) - the DMA never scans a half-written frame, so
     *    there are no tear lines / flicker. The bounce buffer is kept, so the
     *    PSRAM->GDMA path that needed PCLK<=16 MHz is unchanged. */
    ESP_LOGI(TAG, "Initialize LVGL via esp_lvgl_port");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle  = panel,
        .buffer_size   = LCD_H_RES * LCD_V_RES, /* full screen: required for direct mode */
        .double_buffer = false,                 /* the 2 fbs come from the panel itself  */
        .hres          = LCD_H_RES,
        .vres          = LCD_V_RES,
        .color_format  = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .direct_mode = true,                /* draw straight into the panel's fbs     */
            .buff_dma    = false,
            .buff_spiram = true,
            .swap_bytes  = false,               /* parallel RGB panel: no byte swap */
        },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode       = true,              /* bounce buffer in use -> drive vsync via on_bounce_frame_finish */
            .avoid_tearing = true,              /* swap fbs at frame boundary -> no tearing */
        },
    };
    lv_display_t *disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp_rgb failed");
        abort();
    }

    /* 5) Build the dashboard under the LVGL lock. */
    lvgl_port_lock(0);                          /* 0 = wait indefinitely for the mutex */
    dashboard_create();
    lvgl_port_unlock();

    /* Let LVGL render at least one full frame before lighting up, so the very
     * first visible image is the finished dashboard (not an uninitialized fb). */
    vTaskDelay(pdMS_TO_TICKS(150));

    /* 6) Backlight ON. */
    ESP_LOGI(TAG, "Drawing complete, turning on backlight");
    ch422g_set_outputs(&io, CH422G_LCD_RST | CH422G_TP_RST | CH422G_BL);

    ESP_LOGI(TAG, "Speedometer animation should now be running on the screen");

    /* Idle; the RGB DMA keeps refreshing the frame buffer and esp_lvgl_port
     * keeps advancing the animation in its own task. This loop is just a
     * heartbeat so monitor output shows the app is alive. */
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "running #%d", count++);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
