/**
 * @file main.c
 * @brief ST7306 4.2 寸 300x400 全反射 4 色（黑/白/红/青）屏 bring-up 示例。
 *
 * 接线（与 bringup 工程 / 黑白版一致，新板引脚）：
 *   LCD_CS   = GPIO9
 *   LCD_RST  = GPIO10
 *   LCD_TE   = GPIO11   （本示例不读 TE，留空即可）
 *   LCD_SCLK = GPIO12
 *   LCD_MOSI = GPIO13
 *   LCD_RS   = GPIO14   （即 D/C）
 *   VCI      = 3.3V
 *   GND      = GND
 *
 * 前 15 页与黑白屏工程 420-RLCD-400x300-lekang 逐页对齐，
 * 方便两块屏并排上电直接对比；第 16 页起是 4 色专属测试。
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "st7306_color4.h"
#include "color4_demo.h"
#include "board_config.h"

/* 置 1：只跑 4 色专属页（快速确认颜色）
 * 置 0：跑完整 20 页轮播（默认） */
#define DEMO_ONLY_COLOR_PAGES   0

static const char *TAG = "app";

void app_main(void)
{
    const st7306_config_t config = {
        .gpio_dc   = DC_PIN,
        .gpio_res  = RES_PIN,
        .gpio_cs   = CS_PIN,
        .gpio_sclk = SCLK_PIN,
        .gpio_mosi = SDIN_PIN,
        .spi_host  = SPI2_HOST,
        .clock_hz  = 40 * 1000 * 1000,
    };

    st7306_handle_t lcd = st7306_init(&config);
    if (!lcd) {
        ESP_LOGE(TAG, "ST7306 init failed, check wiring and power");
        return;
    }

    /* 动画页需要 HPM；静态页在 LPM 下也能保持画面 */
    st7306_high_power_mode(lcd);
    st7306_display_on(lcd, true);

    ESP_LOGI(TAG, "ST7306 4-color RLCD demo start (%dx%d)",
             ST7306_LCD_WIDTH, ST7306_LCD_HEIGHT);

#if DEMO_ONLY_COLOR_PAGES
    color4_demo_color_pages(lcd);
    ESP_LOGI(TAG, "color pages done, entering full carousel");
#endif

    color4_demo_run(lcd);
}
