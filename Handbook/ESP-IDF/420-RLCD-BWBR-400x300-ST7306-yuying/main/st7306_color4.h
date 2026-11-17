/**
 * @file st7306_color4.h
 * @brief ST7306 4.2 寸 300x400 全反射 4 色（黑/白/红/青）LCD 驱动
 *
 * 直接移植自已验证工程 YDP420HT005-V3 bringup 的 st7306_color4.cc，
 * 初始化序列、显存排布、写点算法保持一致，仅补上在 C 代码中调用的封装。
 *
 * 显存格式：2bit/像素，每字节 4 个像素（MSB 起），一行 300 像素 = 150 字节，
 *           400 行按 2 行合成 1 行数据（上下行各占一个 bit 平面），共 200 数据行。
 *           总显存 = 150 * 200 = 30000 字节。
 *
 * 颜色编码（实测）：bit0 = 青，bit1 = 红，两位都置位才是黑。
 */
#ifndef ST7306_COLOR4_H
#define ST7306_COLOR4_H

#include <stdint.h>
#include <stdbool.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ST7306_LCD_WIDTH        300
#define ST7306_LCD_HEIGHT       400

#define ST7306_DATA_WIDTH       (ST7306_LCD_WIDTH / 2)     /* 150 字节/行 */
#define ST7306_DATA_HEIGHT      (ST7306_LCD_HEIGHT / 2)    /* 200 数据行 */
#define ST7306_BUFFER_LENGTH    (ST7306_DATA_WIDTH * ST7306_DATA_HEIGHT)  /* 30000 */

/* 4 色编码 */
#define ST7306_COLOR_WHITE      0x00
#define ST7306_COLOR_CYAN       0x01
#define ST7306_COLOR_RED        0x02
#define ST7306_COLOR_BLACK      0x03

/* 便捷别名（与 board_color.h 保持一致） */
#define C_WHITE                 ST7306_COLOR_WHITE
#define C_CYAN                  ST7306_COLOR_CYAN
#define C_RED                   ST7306_COLOR_RED
#define C_BLACK                 ST7306_COLOR_BLACK

typedef struct st7306_color4_dev *st7306_handle_t;

typedef struct {
    int gpio_dc;
    int gpio_res;
    int gpio_cs;
    int gpio_sclk;
    int gpio_mosi;
    int spi_host;       /* SPI2_HOST / SPI3_HOST */
    int clock_hz;
} st7306_config_t;

/** 初始化总线、复位并下发 ST7306 初始化序列；失败返回 NULL。 */
st7306_handle_t st7306_init(const st7306_config_t *cfg);

/** 释放设备（通常不需要调用）。 */
void st7306_deinit(st7306_handle_t dev);

/** 用单色填满整块显存（参数是 2bit 颜色值，不是字节）。 */
void st7306_clear(st7306_handle_t dev);
void st7306_fill(st7306_handle_t dev, uint8_t color);

/** 写一个像素，color 取 0..3。 */
void st7306_write_point(st7306_handle_t dev, int x, int y, uint16_t color);

/** 批量填充矩形（含端点），按 2x2 像素块直写显存，比逐点写快约 4 倍。 */
void st7306_fill_rect(st7306_handle_t dev, int x0, int y0, int x1, int y1, uint16_t color);

/** 把整块显存刷到屏上（一次 30000 字节 DMA 传输）。 */
esp_err_t st7306_display(st7306_handle_t dev);

/** 电压/刷新模式 */
void st7306_high_power_mode(st7306_handle_t dev);
void st7306_low_power_mode(st7306_handle_t dev);
void st7306_display_on(st7306_handle_t dev, bool enabled);
void st7306_display_sleep(st7306_handle_t dev, bool enabled);
void st7306_display_inversion(st7306_handle_t dev, bool enabled);

/** 直接读写显存指针，便于整屏位图/快速翻转。 */
uint8_t *st7306_get_buffer(st7306_handle_t dev);
int st7306_get_buffer_length(st7306_handle_t dev);

#ifdef __cplusplus
}
#endif

#endif /* ST7306_COLOR4_H */
