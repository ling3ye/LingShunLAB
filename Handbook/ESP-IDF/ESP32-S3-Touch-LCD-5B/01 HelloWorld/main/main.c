/**
 * "Hello World" on the Waveshare ESP32-S3-Touch-LCD-5B 5" LCD (1024x600).
 *
 * Pipeline in app_main():
 *   1. I2C bus (GPIO8/GPIO9) for the CH422G I/O expander.
 *   2. CH422G: enable EXIO outputs, reset the LCD + touch, keep backlight OFF.
 *   3. RGB LCD panel (esp_lcd) at 1024x600 RGB565, frame buffer in PSRAM.
 *   4. Paint a dark-blue background and centred white "Hello World!" text.
 *   5. Turn the backlight ON (CH422G) so the image becomes visible.
 *   6. Idle - the panel self-refreshes from the PSRAM frame buffer via DMA.
 *
 * No external components: only built-in ESP-IDF APIs + a tiny CH422G driver.
 *
 * Tuning notes (if the screen misbehaves):
 *   - Garbled / shifted image  -> adjust the RGB porch values in panel_cfg.timings.
 *   - Colours look swapped      -> reorder data_gpio_nums (R/G/B bit order).
 *   - Screen stays dark         -> check CH422G I2C / EXIO levels (backlight).
 */
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/i2c_master.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"

#include "ch422g.h"
#include "lcd_draw.h"
#include "font8x16.h"

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
 * Set to 800 to A/B test: on a real 1024-wide panel this reproduces exactly
 * the demo's right-white band (decisive proof the panel is 1024x600). On a
 * genuine 800-wide panel it would instead show a full (possibly shifted) image.
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

/* RGB565 helpers. */
#define RGB565(r, g, b)   ((((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F))
#define COLOR_BG          RGB565(2, 8, 20)     /* dark slate blue */
#define COLOR_FG          RGB565(31, 63, 31)   /* true white — green is 6-bit (0..63), not 5 */

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
    ESP_LOGI(TAG, "Hello World on the Waveshare 5\" 1024x600 LCD");

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
        .bounce_buffer_size_px = 10 * LCD_H_RES,/* SRAM bounce buffer for PSRAM fb */
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
        .flags.fb_in_psram = true,              /* 1.17 MB frame buffer in PSRAM */
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_cfg, &panel));

    /* new_rgb_panel() configures LCD_CAM + GDMA, but call panel_init() to make
     * sure the PSRAM frame-buffer refresh is actually running - otherwise some
     * IDF versions leave the panel emitting no DE and the ST7262 goes blank. */
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    /* 4) Grab the frame buffer and draw. */
    void *fb = NULL;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fb));
    lcd_draw_init((uint16_t *)fb, LCD_H_RES, LCD_V_RES);

    lcd_fill(COLOR_BG);

    const char *msg = "Hello World!";
    const int scale = 5;        /* each font pixel -> 5x5 screen pixels (40x80 per char) */
    const int tw = lcd_text_width(msg, scale);
    const int th = FONT_H * scale;
    lcd_draw_text((LCD_H_RES - tw) / 2, (LCD_V_RES - th) / 2, msg, scale, COLOR_FG);

    /* Let one frame scan out before lighting up (avoids a brief garbage frame). */
    vTaskDelay(pdMS_TO_TICKS(60));

    /* 5) Backlight ON. */
    ESP_LOGI(TAG, "Drawing complete, turning on backlight");
    ch422g_set_outputs(&io, CH422G_LCD_RST | CH422G_TP_RST | CH422G_BL);

    ESP_LOGI(TAG, "\"Hello World!\" should now be on the screen");

    /* 6) Idle; the RGB DMA keeps refreshing the frame buffer. */
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "Hello World! #%d (on screen)", count++);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
