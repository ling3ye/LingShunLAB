/**
 * @file color4_demo.c
 * @brief ST7306 4.2 寸 300x400 全反射 4 色（黑/白/红/青）屏 demo。
 *
 * 页面 1-15 逐页复刻黑白屏工程 420-RLCD-400x300-lekang/main/lcd_demo.c：
 * 相同的页面顺序、相同的版式坐标、相同的驻留时间与帧率上限，
 * 两块屏并排上电时，同一时刻显示的是同一页内容，便于直接对比两种屏。
 *
 * 为了适配 2bit 显存，版面里只有两处必然的差别：
 *   1) 黑白版用 4x4 Bayer 抖动模拟 17 级灰阶；4 色屏没有中间调，
 *      这里统一量化为 白 -> 青 -> 红 -> 黑 四档（gray_rect/gray_circle 同名保留）。
 *   2) 黑白的反色、星空、生命游戏等页，用红/青做重点色点缀。
 *
 * 页面 16 起是 4 色屏专属的多色测试，黑白屏没有对应内容。
 */
#include "color4_demo.h"
#include "st73xx_ui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#define W ST7306_LCD_WIDTH
#define H ST7306_LCD_HEIGHT

#define BLACK ST7306_COLOR_BLACK
#define WHITE ST7306_COLOR_WHITE
#define RED   ST7306_COLOR_RED
#define CYAN  ST7306_COLOR_CYAN

#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const char *TAG = "color4_demo";

/* 5x7 ASCII 字体，字形表与黑白版完全一致（列编码，bit0 在顶部）。 */
static const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5f,0x00,0x00},
    {0x00,0x07,0x00,0x07,0x00},{0x14,0x7f,0x14,0x7f,0x14},
    {0x24,0x2a,0x7f,0x2a,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1c,0x22,0x41,0x00},{0x00,0x41,0x22,0x1c,0x00},
    {0x14,0x08,0x3e,0x08,0x14},{0x08,0x08,0x3e,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e},
    {0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3e},{0x7e,0x11,0x11,0x11,0x7e},
    {0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},
    {0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},
    {0x7f,0x09,0x09,0x09,0x01},{0x3e,0x41,0x49,0x49,0x7a},
    {0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},
    {0x20,0x40,0x41,0x3f,0x01},{0x7f,0x08,0x14,0x22,0x41},
    {0x7f,0x40,0x40,0x40,0x40},{0x7f,0x02,0x0c,0x02,0x7f},
    {0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},
    {0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},
    {0x7f,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7f,0x01,0x01},{0x3f,0x40,0x40,0x40,0x3f},
    {0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},
    {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43},{0x00,0x7f,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7f,0x00},
    {0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7f,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7f},{0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7e,0x09,0x01,0x02},{0x0c,0x52,0x52,0x52,0x3e},
    {0x7f,0x08,0x04,0x04,0x78},{0x00,0x44,0x7d,0x40,0x00},
    {0x20,0x40,0x44,0x3d,0x00},{0x7f,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7f,0x40,0x00},{0x7c,0x04,0x18,0x04,0x78},
    {0x7c,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7c,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7c},
    {0x7c,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3f,0x44,0x40,0x20},{0x3c,0x40,0x40,0x20,0x7c},
    {0x1c,0x20,0x40,0x20,0x1c},{0x3c,0x40,0x30,0x40,0x3c},
    {0x44,0x28,0x10,0x28,0x44},{0x0c,0x50,0x50,0x50,0x3c},
    {0x44,0x64,0x54,0x4c,0x44},{0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7f,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},
    {0x08,0x04,0x08,0x10,0x08},{0x00,0x00,0x00,0x00,0x00}
};
/* --------------------------------------------------------------- */
/* 文本与图元封装                                                    */
/* --------------------------------------------------------------- */

static void text_char(st7306_handle_t d, int x, int y, char c, int scale, uint16_t color)
{
    if ((unsigned char)c < 32 || (unsigned char)c > 127) c = '?';
    const uint8_t *glyph = font5x7[(unsigned char)c - 32];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (glyph[col] & (1U << row)) {
                if (scale == 1) {
                    st7306_write_point(d, x + col, y + row, color);
                } else {
                    ui_draw_filled_rectangle(d, x + col * scale, y + row * scale,
                        x + (col + 1) * scale - 1, y + (row + 1) * scale - 1, color);
                }
            }
        }
    }
}

static void text(st7306_handle_t d, int x, int y, const char *s, int scale, uint16_t color)
{
    while (*s) {
        text_char(d, x, y, *s++, scale, color);
        x += 6 * scale;
    }
}

static int text_width(const char *s, int scale)
{
    const int n = (int)strlen(s);
    return n ? (n * 6 - 1) * scale : 0;
}

static void text_center(st7306_handle_t d, int y, const char *s, int scale, uint16_t color)
{
    text(d, (W - text_width(s, scale)) / 2, y, s, scale, color);
}

static void hline(st7306_handle_t d, int x0, int x1, int y, uint16_t color)
{
    ui_draw_fast_hline(d, x0, y, x1 - x0 + 1, color);
}

static void vline(st7306_handle_t d, int x, int y0, int y1, uint16_t color)
{
    ui_draw_fast_vline(d, x, y0, y1 - y0 + 1, color);
}

static void rect(st7306_handle_t d, int x0, int y0, int x1, int y1, uint16_t color)
{
    ui_draw_filled_rectangle(d, x0, y0, x1, y1, color);
}

/* 黑白版用 4x4 Bayer 抖动造 17 级灰；4 色屏没有中间调，
 * 这里同名函数直接画青色实心块，页面代码就能与黑白版逐行对应。 */
static void gray_rect(st7306_handle_t d, int x0, int y0, int x1, int y1, int level)
{
    if (level <= 0) return;
    rect(d, x0, y0, x1, y1, CYAN);
}

static void gray_circle(st7306_handle_t d, int cx, int cy, int r, int level)
{
    if (level <= 0) return;
    ui_draw_filled_circle(d, cx, cy, r, CYAN);
}

/* 17 级灰度量化到 4 色：白 -> 青 -> 红 -> 黑 */
static uint16_t quantize_level(int level)
{
    if (level < 4) return WHITE;
    if (level < 9) return CYAN;
    if (level < 14) return RED;
    return BLACK;
}

static uint32_t rng_state = 0x7306u;
static uint32_t demo_rand(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static int wave(int tick, int period)
{
    const int p = tick % (period * 2);
    return p < period ? p : period * 2 - p;
}

/* 整数平方根（牛顿迭代），用于在逻辑坐标里画正圆 */
static int int_sqrt(int v)
{
    if (v <= 0) return 0;
    int x = v;
    int y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + v / x) / 2;
    }
    return x;
}

/* --------------------------------------------------------------- */
/* 页眉 / 页脚                                                       */
/* --------------------------------------------------------------- */

static int g_total_pages = 15;
static int g_page_index = 0;   /* 由轮播器设置，chrome 据此显示页码与进度点 */

static void chrome(st7306_handle_t d, const char *title, const char *subtitle)
{
    const int page = g_page_index;
    char buf[24];
    rect(d, 0, 0, W - 1, 31, BLACK);
    text(d, 8, 8, title, 2, WHITE);
    snprintf(buf, sizeof(buf), "%02d/%d", page + 1, g_total_pages);
    text(d, 251, 11, buf, 1, WHITE);
    text(d, 8, 39, subtitle, 1, BLACK);
    hline(d, 0, W - 1, 51, BLACK);
    hline(d, 8, W - 9, 376, BLACK);
    text(d, 8, 384, "ST7306 RLCD 4-COLOR TEST SUITE", 1, BLACK);

    /* 进度点自适应：15 页以内沿用黑白版的 6px 间距，超过后压缩到 3px */
    const int count = g_total_pages;
    const int step = (count > 15) ? 3 : 6;
    const int start = W - 8 - count * step;
    for (int i = 0; i < count; i++) {
        const int x = start + i * step;
        if (i == page) rect(d, x, 385, x + 2, 391, BLACK);
        else ui_draw_rectangle(d, x, 385, x + 2, 391, BLACK);
    }
}
/* --------------------------------------------------------------- */
/* 1. HELLO WORLD                                                   */
/* --------------------------------------------------------------- */

static void page_hello(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame; (void)elapsed;
    rect(d, 0, 0, W - 1, H - 1, CYAN);
    rect(d, 12, 12, 287, 387, WHITE);
    ui_draw_rectangle(d, 12, 12, 287, 387, BLACK);
    ui_draw_rectangle(d, 16, 16, 283, 383, RED);
    text_center(d, 54, "ST7306  /  RLCD", 2, BLACK);
    hline(d, 42, 258, 82, BLACK);
    text_center(d, 112, "HELLO", 7, BLACK);
    text_center(d, 174, "WORLD!", 7, RED);
    text_center(d, 247, "300 x 400  4-COLOR", 2, BLACK);
    text_center(d, 278, "BLACK WHITE RED CYAN", 1, CYAN);
    ui_draw_filled_circle(d, 150, 330, 28, CYAN);
    ui_draw_circle(d, 150, 330, 29, BLACK);
    ui_draw_filled_circle(d, 140, 321, 3, BLACK);
    ui_draw_filled_circle(d, 160, 321, 3, BLACK);
    ui_draw_line(d, 136, 338, 142, 343, BLACK);
    ui_draw_line(d, 142, 343, 158, 343, BLACK);
    ui_draw_line(d, 158, 343, 164, 338, BLACK);
}

/* --------------------------------------------------------------- */
/* 2. 17 级灰度 -> 4 色量化                                          */
/* --------------------------------------------------------------- */

static void page_gray(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame; (void)elapsed;
    chrome(d, "4-COLOR QUANTIZE", "17 GRAY STEPS -> WHITE/CYAN/RED/BLACK");

    for (int i = 0; i < 17; i++) {
        const int x = 14 + i * 16;
        rect(d, x, 72, x + 14, 128, quantize_level(i));
        ui_draw_rectangle(d, x, 72, x + 14, 128, BLACK);
    }
    text(d, 14, 136, "0%", 1, BLACK);
    text(d, 139, 136, "50", 1, BLACK);
    text(d, 259, 136, "100%", 1, BLACK);

    text(d, 14, 159, "QUANTIZED GRADIENT", 1, BLACK);
    for (int y = 0; y < 36; y++) {
        for (int x = 0; x < 272; x++) {
            st7306_write_point(d, 14 + x, 176 + y, quantize_level(x / 17));
        }
    }
    ui_draw_rectangle(d, 13, 175, 286, 212, BLACK);

    text(d, 14, 230, "ONLY 4 COLORS - NO DITHER", 1, BLACK);
    ui_draw_filled_circle(d, 58, 294, 39, WHITE);
    ui_draw_circle(d, 58, 294, 40, BLACK);
    gray_circle(d, 150, 294, 39, 8);
    ui_draw_circle(d, 150, 294, 40, BLACK);
    ui_draw_filled_circle(d, 242, 294, 39, RED);
    ui_draw_circle(d, 242, 294, 40, BLACK);
    text_center(d, 347, "2 BITS PER PIXEL = 4 COLORS", 1, BLACK);
}

/* --------------------------------------------------------------- */
/* 3. 几何图形（与黑白版同坐标）                                      */
/* --------------------------------------------------------------- */

static void page_geometry(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame; (void)elapsed;
    chrome(d, "GEOMETRY", "LINES / CIRCLES / TRIANGLES / POLYGONS");

    ui_draw_line(d, 12, 65, 285, 350, RED);
    ui_draw_line(d, 285, 65, 12, 350, CYAN);

    ui_draw_filled_rectangle(d, 20, 75, 126, 164, CYAN);
    ui_draw_rectangle(d, 20, 75, 126, 164, BLACK);
    rect(d, 43, 97, 104, 139, BLACK);

    ui_draw_circle(d, 213, 122, 48, BLACK);
    ui_draw_circle(d, 213, 122, 38, RED);
    ui_draw_filled_circle(d, 213, 122, 27, CYAN);

    ui_draw_triangle(d, 24, 323, 87, 188, 143, 323, BLACK);
    ui_draw_filled_triangle(d, 176, 310, 219, 212, 271, 310, RED);

    const int p[] = {152, 77, 168, 95, 162, 117, 143, 117, 136, 95};
    ui_draw_filled_polygon(d, p, 5, BLACK);
    text_center(d, 348, "BRESENHAM + SCANLINE FILL", 1, BLACK);
}

/* --------------------------------------------------------------- */
/* 4. 像素网格                                                      */
/* --------------------------------------------------------------- */

static void page_grid(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame; (void)elapsed;
    chrome(d, "PIXEL GRID", "CHECK EDGES, ASPECT RATIO AND DIAGONALS");

    ui_draw_rectangle(d, 0, 58, 299, 367, BLACK);
    for (int x = 19; x < W; x += 20) vline(d, x, 59, 366, CYAN);
    for (int y = 78; y < 367; y += 20) hline(d, 1, 298, y, CYAN);
    ui_draw_line(d, 0, 58, 299, 367, BLACK);
    ui_draw_line(d, 299, 58, 0, 367, BLACK);
    ui_draw_circle(d, 150, 213, 100, RED);
    ui_draw_rectangle(d, 50, 113, 250, 313, BLACK);

    rect(d, 0, 58, 5, 63, BLACK);
    rect(d, 294, 58, 299, 63, BLACK);
    rect(d, 0, 362, 5, 367, BLACK);
    rect(d, 294, 362, 299, 367, BLACK);

    rect(d, 145, 208, 155, 218, WHITE);
    ui_draw_rectangle(d, 145, 208, 155, 218, BLACK);
    rect(d, 148, 211, 152, 215, RED);
}

/* --------------------------------------------------------------- */
/* 5. 文字与字体                                                    */
/* --------------------------------------------------------------- */

static void page_text(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame; (void)elapsed;
    chrome(d, "TEXT & FONT", "BUILT-IN 5x7 ASCII / INTEGER SCALING");

    text_center(d, 69, "BIG TEXT 2X", 2, BLACK);
    text_center(d, 96, "HUGE 4X", 4, RED);
    hline(d, 18, 281, 135, BLACK);

    text(d, 14, 148, "Regular: ABCDEFGHIJKLM", 1, BLACK);
    text(d, 14, 162, "         NOPQRSTUVWXYZ", 1, BLACK);
    text(d, 14, 179, "Lower:  abcdefghijklm", 1, CYAN);
    text(d, 14, 193, "        nopqrstuvwxyz", 1, CYAN);
    text(d, 14, 214, "Digits: 0123456789", 2, BLACK);
    text(d, 14, 241, "Symbols: !#$%&*+-/<=>?", 1, BLACK);

    rect(d, 14, 265, 285, 304, RED);
    text_center(d, 273, "COLOR TEXT PANEL", 2, WHITE);

    rect(d, 14, 318, 285, 348, BLACK);
    text_center(d, 327, "REVERSED TEXT", 2, WHITE);
    text_center(d, 359, "NO EXTERNAL FONT REQUIRED", 1, BLACK);
}
/* --------------------------------------------------------------- */
/* 6. 桌面 UI 仪表盘                                                */
/* --------------------------------------------------------------- */

static void draw_toggle(st7306_handle_t d, int x, int y, bool on)
{
    ui_draw_rectangle(d, x, y, x + 35, y + 16, BLACK);
    if (on) {
        rect(d, x + 2, y + 2, x + 32, y + 14, RED);
        ui_draw_filled_circle(d, x + 26, y + 8, 5, WHITE);
    } else {
        ui_draw_filled_circle(d, x + 9, y + 8, 5, BLACK);
    }
}

static void page_ui(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)elapsed;
    chrome(d, "DESKTOP UI", "CARDS / GAUGES / LIVE WIDGETS");

    gray_rect(d, 17, 67, 188, 191, 5);
    rect(d, 13, 63, 184, 187, WHITE);
    ui_draw_rectangle(d, 13, 63, 184, 187, BLACK);
    text(d, 23, 74, "ENVIRONMENT", 2, BLACK);
    hline(d, 23, 174, 96, BLACK);
    text(d, 23, 109, "23.6", 4, BLACK);
    text(d, 107, 109, "C", 2, RED);
    text(d, 23, 145, "HUMIDITY  48%", 1, BLACK);
    text(d, 23, 161, "PRESSURE  1013", 1, BLACK);

    gray_rect(d, 200, 67, 290, 191, 7);
    rect(d, 196, 63, 286, 187, WHITE);
    ui_draw_rectangle(d, 196, 63, 286, 187, BLACK);
    text(d, 207, 74, "POWER", 1, BLACK);
    const int battery = 20 + wave(frame, 40) * 70 / 40;
    ui_draw_rectangle(d, 210, 99, 267, 121, BLACK);
    rect(d, 267, 104, 271, 116, BLACK);
    /* 电量条：>60% 青，30~60% 红，<30% 黑（低电警示） */
    const uint16_t bar_color = battery > 60 ? CYAN : (battery > 30 ? RED : BLACK);
    rect(d, 213, 102, 213 + battery * 50 / 100, 118, bar_color);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d%%", battery);
    text(d, 241 - text_width(buf, 2) / 2, 134, buf, 2, BLACK);
    text(d, 207, 160, "SOLAR  ON", 1, CYAN);

    rect(d, 13, 204, 286, 294, WHITE);
    ui_draw_rectangle(d, 13, 204, 286, 294, BLACK);
    text(d, 24, 216, "SYSTEM ACTIVITY", 1, BLACK);
    for (int i = 0; i < 3; i++) {
        const int value = 12 + wave(frame + i * 17, 50) * 180 / 50;
        const uint16_t bar = (i == 0) ? CYAN : ((i == 1) ? RED : BLACK);
        text(d, 24, 240 + i * 16, i == 0 ? "CPU" : (i == 1 ? "MEM" : "SPI"), 1, BLACK);
        gray_rect(d, 62, 240 + i * 16, 256, 248 + i * 16, 3);
        rect(d, 62, 240 + i * 16, 62 + value, 248 + i * 16, bar);
    }
    text(d, 22, 316, "WIFI", 1, BLACK);
    draw_toggle(d, 63, 311, true);
    text(d, 118, 316, "SYNC", 1, BLACK);
    draw_toggle(d, 160, 311, (frame / 12) & 1);
    text(d, 215, 316, "LIVE", 1, BLACK);
    if ((frame / 4) & 1) ui_draw_filled_circle(d, 276, 319, 5, RED);
    else ui_draw_circle(d, 276, 319, 5, BLACK);
    text_center(d, 351, "A TINY REFLECTIVE DASHBOARD", 1, BLACK);
}

/* --------------------------------------------------------------- */
/* 7. 帧率基准                                                      */
/* --------------------------------------------------------------- */

typedef struct {
    uint32_t last_us;
    uint32_t average_us;
    uint16_t history[54];
    int history_pos;
} perf_t;

static perf_t perf;

static void perf_record(uint32_t elapsed_us)
{
    perf.last_us = elapsed_us;
    perf.average_us = perf.average_us ? (perf.average_us * 7 + elapsed_us) / 8 : elapsed_us;
    perf.history[perf.history_pos] = (uint16_t)(elapsed_us / 100);
    perf.history_pos = (perf.history_pos + 1) % ARRAY_LEN(perf.history);
}

static void page_fps(st7306_handle_t d, int frame, int64_t elapsed)
{
    chrome(d, "FRAME RATE", "UNTHROTTLED FULL-FRAME DRAW + SPI BENCHMARK");
    char buf[40];
    const uint32_t avg = perf.average_us ? perf.average_us : 1;
    const uint32_t fps10 = 10000000UL / avg;
    snprintf(buf, sizeof(buf), "%lu.%lu FPS", (unsigned long)(fps10 / 10), (unsigned long)(fps10 % 10));
    text_center(d, 66, buf, 4, BLACK);

    snprintf(buf, sizeof(buf), "LAST %lu.%lu ms   AVG %lu.%lu ms",
        (unsigned long)(perf.last_us / 1000), (unsigned long)((perf.last_us % 1000) / 100),
        (unsigned long)(avg / 1000), (unsigned long)((avg % 1000) / 100));
    text_center(d, 108, buf, 1, BLACK);

    text(d, 16, 132, "UPDATE TIME HISTORY", 1, BLACK);
    ui_draw_rectangle(d, 15, 148, 285, 254, BLACK);
    hline(d, 16, 284, 174, CYAN);
    hline(d, 16, 284, 201, CYAN);
    hline(d, 16, 284, 227, CYAN);

    int previous_x = -1, previous_y = -1;
    for (int i = 0; i < ARRAY_LEN(perf.history); i++) {
        const int index = (perf.history_pos + i) % ARRAY_LEN(perf.history);
        int y = 250 - perf.history[index] * 95 / 150;
        if (y < 151) y = 151;
        if (y > 250) y = 250;
        const int x = 20 + i * 5;
        ui_draw_filled_circle(d, x, y, 1, RED);
        if (previous_x >= 0) ui_draw_line(d, previous_x, previous_y, x, y, RED);
        previous_x = x;
        previous_y = y;
    }

    snprintf(buf, sizeof(buf), "FRAME: %d", frame + 1);
    text(d, 17, 271, buf, 1, BLACK);
    text(d, 17, 291, "30000 BYTES PER UPDATE", 2, BLACK);
    text(d, 17, 317, "SPI: 40 MHz   PANEL: 32 Hz", 1, BLACK);
    text(d, 17, 337, "30000 B = 300 x 400 x 2bit / 8", 1, BLACK);
    text(d, 17, 351, "BW PANEL NEEDS 15000 B ONLY.", 1, BLACK);
    const int scan = 20 + (frame * 7) % 260;
    vline(d, scan, 149, 253, CYAN);
    (void)elapsed;
}
/* --------------------------------------------------------------- */
/* 8. 精细图案 / 闪烁检查                                            */
/* --------------------------------------------------------------- */

static void page_patterns(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)elapsed;
    chrome(d, "FINE PATTERNS", "1/2/4 PIXEL CHECKERS + COLOR Fringe TEST");
    const int phase = frame & 1;
    static const int boxes[4][4] = {
        {12, 66, 146, 184}, {153, 66, 287, 184},
        {12, 194, 146, 332}, {153, 194, 287, 332}
    };
    /* 四个象限分别用 黑/白、红/白、青/白、红/青 棋盘，
     * 能同时看出 1:1 子像素混色是否出现彩色边缘。 */
    for (int b = 0; b < 4; b++) {
        for (int y = boxes[b][1]; y <= boxes[b][3]; y++) {
            for (int x = boxes[b][0]; x <= boxes[b][2]; x++) {
                bool flip = false;
                int cell = 1;
                if (b == 0 || b == 1) {
                    flip = ((x ^ y) & 1) == phase;
                } else if (b == 2) {
                    cell = 2;
                    flip = (((x / cell) ^ (y / cell)) & 1) == phase;
                } else {
                    cell = 4;
                    flip = (((x / cell) ^ (y / cell)) & 1) == phase;
                }
                uint16_t c = WHITE;
                if (b == 0) c = flip ? BLACK : WHITE;
                if (b == 1) c = flip ? RED : WHITE;
                if (b == 2) c = flip ? CYAN : WHITE;
                if (b == 3) c = flip ? CYAN : RED;
                st7306_write_point(d, x, y, c);
            }
        }
        ui_draw_rectangle(d, boxes[b][0] - 1, boxes[b][1] - 1,
                          boxes[b][2] + 1, boxes[b][3] + 1, BLACK);
    }
    text_center(d, 343, "BW / RED-W / CYAN-W / RED-CYAN", 1, BLACK);
    text_center(d, 358, phase ? "PHASE B - CHECK FOR FLICKER"
                              : "PHASE A - CHECK FOR FLICKER", 1, BLACK);
}

/* --------------------------------------------------------------- */
/* 9. 弹跳方块                                                      */
/* --------------------------------------------------------------- */

static struct { int x, y, vx, vy, hits; } bounce;

static void bounce_reset(void)
{
    bounce = (typeof(bounce)){25, 85, 3, 2, 0};
}

static void page_bounce(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)elapsed;
    chrome(d, "BOUNCE TEST", "MOVEMENT, COLLISIONS AND PANEL GHOSTING");
    const int left = 10, right = 289, top = 65, bottom = 355;

    ui_draw_rectangle(d, left, top, right, bottom, BLACK);
    for (int y = top + 10; y < bottom; y += 20) hline(d, left + 1, right - 1, y, CYAN);

    if (frame > 0) {
        bounce.x += bounce.vx;
        bounce.y += bounce.vy;
        if (bounce.x <= left + 2 || bounce.x + 91 >= right - 1) {
            bounce.vx = -bounce.vx;
            bounce.x += bounce.vx * 2;
            bounce.hits++;
        }
        if (bounce.y <= top + 2 || bounce.y + 43 >= bottom - 1) {
            bounce.vy = -bounce.vy;
            bounce.y += bounce.vy * 2;
            bounce.hits++;
        }
    }

    gray_rect(d, bounce.x + 5, bounce.y + 5, bounce.x + 96, bounce.y + 48, 5);
    rect(d, bounce.x, bounce.y, bounce.x + 91, bounce.y + 43, WHITE);
    ui_draw_rectangle(d, bounce.x, bounce.y, bounce.x + 91, bounce.y + 43, BLACK);
    text(d, bounce.x + 9, bounce.y + 10, "RLCD", 3, RED);

    char buf[24];
    snprintf(buf, sizeof(buf), "WALL HITS: %d", bounce.hits);
    text_center(d, 360, buf, 1, BLACK);
}

/* --------------------------------------------------------------- */
/* 10. 星空                                                         */
/* --------------------------------------------------------------- */

#define STAR_COUNT 56
static struct { int x, y, z; } stars[STAR_COUNT];

static void star_reset(int i, bool initial)
{
    stars[i].x = (int)(demo_rand() % 401) - 200;
    stars[i].y = (int)(demo_rand() % 401) - 200;
    stars[i].z = initial ? 30 + (int)(demo_rand() % 220) : 250;
}

static void page_stars(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame; (void)elapsed;
    rect(d, 0, 0, W - 1, H - 1, BLACK);
    text(d, 10, 12, "STARFIELD", 2, CYAN);
    text(d, 190, 17, "56 STARS / HPM", 1, WHITE);
    hline(d, 10, 289, 38, WHITE);

    for (int i = 0; i < STAR_COUNT; i++) {
        const int old_z = stars[i].z;
        stars[i].z -= 7;
        if (stars[i].z < 20) star_reset(i, false);
        const int z = stars[i].z;
        const int x = 150 + stars[i].x * 110 / z;
        const int y = 207 + stars[i].y * 110 / z;
        const int ox = 150 + stars[i].x * 110 / old_z;
        const int oy = 207 + stars[i].y * 110 / old_z;
        if (x > 2 && x < 297 && y > 44 && y < 370) {
            /* 近处星用红，远处用白，速度感更明显 */
            const uint16_t c = (z < 80) ? RED : WHITE;
            ui_draw_line(d, ox, oy, x, y, c);
            if (z < 80) ui_draw_filled_circle(d, x, y, 2, c);
            else st7306_write_point(d, x, y, c);
        }
    }
    hline(d, 10, 289, 377, WHITE);
    text(d, 10, 385, "WARP THROUGH A 4-COLOR GALAXY", 1, WHITE);
}
/* --------------------------------------------------------------- */
/* 11. 生命游戏                                                      */
/* --------------------------------------------------------------- */

#define LIFE_W 34
#define LIFE_H 34

static uint8_t life[LIFE_H][LIFE_W];
static uint8_t life_next[LIFE_H][LIFE_W];
static int generation;

static void life_seed(void)
{
    memset(life, 0, sizeof(life));
    rng_state = 0x42030040u;
    for (int y = 2; y < LIFE_H - 2; y++) {
        for (int x = 2; x < LIFE_W - 2; x++) {
            life[y][x] = (demo_rand() % 100) < 28;
        }
    }
    generation = 0;
}

static void life_step(void)
{
    for (int y = 0; y < LIFE_H; y++) {
        for (int x = 0; x < LIFE_W; x++) {
            int neighbors = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (!dx && !dy) continue;
                    neighbors += life[(y + dy + LIFE_H) % LIFE_H][(x + dx + LIFE_W) % LIFE_W];
                }
            }
            life_next[y][x] = (neighbors == 3) || (neighbors == 2 && life[y][x]);
        }
    }
    memcpy(life, life_next, sizeof(life));
    generation++;
}

static void page_life(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)elapsed;
    chrome(d, "GAME OF LIFE", "CONWAY AUTOMATON / TORUS WORLD");
    if (frame > 0) {
        life_step();
    }

    const int cell = 8, ox = 14, oy = 62;
    gray_rect(d, ox - 3, oy - 3, ox + LIFE_W * cell + 1, oy + LIFE_H * cell + 1, 3);
    ui_draw_rectangle(d, ox - 2, oy - 2, ox + LIFE_W * cell, oy + LIFE_H * cell, BLACK);

    for (int y = 0; y < LIFE_H; y++) {
        for (int x = 0; x < LIFE_W; x++) {
            if (life[y][x]) {
                /* 新生细胞用红、老细胞用黑，一眼看出演化层次 */
                const uint16_t c = (generation < 8) ? RED : BLACK;
                rect(d, ox + x * cell + 1, oy + y * cell + 1,
                     ox + x * cell + 6, oy + y * cell + 6, c);
            }
        }
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "GENERATION %d", generation);
    text_center(d, 352, buf, 1, BLACK);
}

/* --------------------------------------------------------------- */
/* 12. 运行时钟                                                     */
/* --------------------------------------------------------------- */

static void page_clock(st7306_handle_t d, int frame, int64_t elapsed)
{
    chrome(d, "UPTIME CLOCK", "ESP-TIMER / NO RTC OR NETWORK REQUIRED");
    const int total = (int)(elapsed / 1000000);
    const int seconds = total % 60;
    const int minutes = (total / 60) % 60;
    const int hours = total / 3600;

    const int cx = 150, cy = 177, radius = 101;
    ui_draw_circle(d, cx, cy, radius, BLACK);
    ui_draw_circle(d, cx, cy, radius - 2, RED);

    static const int8_t sx[12] = {0,50,87,100,87,50,0,-50,-87,-100,-87,-50};
    static const int8_t sy[12] = {-100,-87,-50,0,50,87,100,87,50,0,-50,-87};
    for (int i = 0; i < 12; i++) {
        ui_draw_filled_circle(d, cx + sx[i] * 88 / 100, cy + sy[i] * 88 / 100, 3, BLACK);
    }
    /* 秒针红、分针黑，4 色屏上层次比单色更清楚 */
    const int sk = (seconds / 5) % 12;
    ui_draw_line(d, cx, cy, cx + sx[sk] * 76 / 100, cy + sy[sk] * 76 / 100, RED);
    const int mk = (minutes / 5) % 12;
    ui_draw_line(d, cx, cy, cx + sx[mk] * 53 / 100, cy + sy[mk] * 53 / 100, BLACK);
    ui_draw_filled_circle(d, cx, cy, 5, CYAN);

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
    text_center(d, 301, buf, 4, BLACK);
    snprintf(buf, sizeof(buf), "RENDER FRAME %d", frame + 1);
    text_center(d, 343, buf, 1, BLACK);
    text_center(d, 359, "4 COLORS DO NOT MEAN BORING", 1, CYAN);
}

/* --------------------------------------------------------------- */
/* 13. 复古桌面                                                     */
/* --------------------------------------------------------------- */

static void tiny_mac(st7306_handle_t d, int x, int y)
{
    ui_draw_rectangle(d, x, y, x + 37, y + 47, BLACK);
    rect(d, x + 4, y + 4, x + 33, y + 31, BLACK);
    rect(d, x + 6, y + 6, x + 31, y + 29, WHITE);
    rect(d, x + 10, y + 11, x + 12, y + 14, RED);
    rect(d, x + 21, y + 11, x + 23, y + 14, CYAN);
    ui_draw_line(d, x + 12, y + 23, x + 16, y + 25, BLACK);
    ui_draw_line(d, x + 16, y + 25, x + 22, y + 25, BLACK);
    ui_draw_line(d, x + 22, y + 25, x + 26, y + 23, BLACK);
    rect(d, x + 7, y + 38, x + 23, y + 41, BLACK);
}

static void page_retro(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame; (void)elapsed;
    gray_rect(d, 0, 0, W - 1, H - 1, 3);
    rect(d, 0, 0, W - 1, 21, WHITE);
    hline(d, 0, W - 1, 22, BLACK);
    text(d, 9, 7, "*  File  Edit  View  Special", 1, BLACK);
    text(d, 258, 7, "12:00", 1, BLACK);

    text(d, 181, 34, "RLCD OS", 2, RED);
    text(d, 184, 55, "System 1.0", 1, BLACK);
    tiny_mac(d, 228, 77);

    gray_rect(d, 17, 39, 212, 278, 8);
    rect(d, 12, 34, 207, 273, WHITE);
    ui_draw_rectangle(d, 12, 34, 207, 273, BLACK);
    rect(d, 13, 35, 206, 51, BLACK);
    text(d, 54, 39, "CONTROL PANEL", 1, WHITE);
    ui_draw_rectangle(d, 16, 38, 20, 47, WHITE);
    text(d, 24, 65, "Desktop Pattern", 1, BLACK);

    for (int y = 0; y < 36; y++) {
        for (int x = 0; x < 56; x++) {
            if (((x / 4) ^ (y / 4)) & 1) {
                st7306_write_point(d, 25 + x, 82 + y, RED);
            }
        }
    }
    gray_rect(d, 94, 82, 177, 117, 6);
    ui_draw_rectangle(d, 24, 81, 81, 118, BLACK);
    ui_draw_rectangle(d, 93, 81, 178, 118, BLACK);

    hline(d, 22, 196, 130, BLACK);
    text(d, 24, 141, "Double-Click Speed", 1, BLACK);
    for (int i = 0; i < 3; i++) {
        ui_draw_rectangle(d, 26 + i * 51, 163, 37 + i * 51, 174, BLACK);
        if (i == 1) rect(d, 28 + i * 51, 165, 35 + i * 51, 172, BLACK);
    }
    text(d, 24, 190, "Key Repeat Rate", 1, BLACK);
    gray_rect(d, 25, 211, 187, 218, 4);
    rect(d, 25, 211, 110, 218, CYAN);
    ui_draw_rectangle(d, 25, 210, 188, 219, BLACK);
    text(d, 24, 237, "Reflective. Silent. Colorful.", 1, BLACK);

    rect(d, 24, 294, 195, 353, WHITE);
    ui_draw_rectangle(d, 24, 294, 195, 353, BLACK);
    rect(d, 25, 295, 194, 310, BLACK);
    text(d, 65, 299, "HELLO, WORLD!", 1, WHITE);
    text(d, 36, 322, "4 colors. Infinite ideas.", 1, BLACK);
    text(d, 36, 338, "OK", 1, RED);
    ui_draw_rectangle(d, 135, 324, 180, 344, BLACK);

    text(d, 236, 348, "TRASH", 1, BLACK);
    ui_draw_rectangle(d, 244, 305, 265, 337, BLACK);
    hline(d, 240, 269, 302, BLACK);
    vline(d, 250, 309, 333, CYAN);
    vline(d, 258, 309, 333, CYAN);
}
/* --------------------------------------------------------------- */
/* 14. 反色 / 极性对比                                              */
/* --------------------------------------------------------------- */

static void page_inverse(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)elapsed;
    chrome(d, "POLARITY", "INVERSION / CONTRAST / COLOR SWAP");
    const bool invert = (frame / 2) & 1;

    if (!invert) {
        rect(d, 12, 66, 287, 346, BLACK);
        text_center(d, 92, "BLACK", 6, WHITE);
        text_center(d, 155, "ON", 4, CYAN);
        text_center(d, 205, "WHITE", 6, WHITE);
        rect(d, 42, 276, 257, 324, CYAN);
        text_center(d, 289, "REVERSED", 3, BLACK);
    } else {
        ui_draw_rectangle(d, 12, 66, 287, 346, BLACK);
        text_center(d, 92, "WHITE", 6, BLACK);
        text_center(d, 155, "ON", 4, RED);
        text_center(d, 205, "BLACK", 6, BLACK);
        rect(d, 42, 276, 257, 324, RED);
        text_center(d, 289, "REVERSED", 3, WHITE);
    }
    text_center(d, 357, invert ? "PHASE B - HW INVERSION ON"
                               : "PHASE A - HW INVERSION OFF", 1, BLACK);
}

/* --------------------------------------------------------------- */
/* 15. 旋转                                                         */
/* --------------------------------------------------------------- */

static void page_rotation(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame;
    /* 4 色屏显存排布固定（MX=1, DO=1），旋转只能在绘制层做。
     * 这一页用方向箭头 + 四角标记验证用户看到的朝向是否与预期一致。 */
    static const int angles[4] = {0, 90, 180, 270};
    const int rotation = (int)((elapsed / 3000000) & 3);
    const int width = (rotation & 1) ? H : W;
    const int height = (rotation & 1) ? W : H;

    /* 逻辑坐标 -> 物理坐标（顺时针旋转）
     *   0   : (lx, ly)
     *   90  : 逻辑画布 400x300 -> 物理 300x400，左上角转到右上角
     *   180 : 整体倒置
     *   270 : 逻辑左上角转到物理左下角 */
    #define MAP_X(lx, ly)                                                       \
        (rotation == 0 ? (lx) : rotation == 1 ? (W - 1 - (ly)) :                \
         rotation == 2 ? (W - 1 - (lx)) : (ly))
    #define MAP_Y(lx, ly)                                                       \
        (rotation == 0 ? (ly) : rotation == 1 ? (lx) :                          \
         rotation == 2 ? (H - 1 - (ly)) : (H - 1 - (lx)))

    /* 逻辑画布外框 */
    for (int i = 0; i < width; i++) {
        st7306_write_point(d, MAP_X(i, 0), MAP_Y(i, 0), BLACK);
        st7306_write_point(d, MAP_X(i, height - 1), MAP_Y(i, height - 1), BLACK);
    }
    for (int i = 0; i < height; i++) {
        st7306_write_point(d, MAP_X(0, i), MAP_Y(0, i), BLACK);
        st7306_write_point(d, MAP_X(width - 1, i), MAP_Y(width - 1, i), BLACK);
    }

    /* 顶栏 */
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < width; x++) {
            st7306_write_point(d, MAP_X(x, y), MAP_Y(x, y), BLACK);
        }
    }

    const int cx = width / 2;
    const int cy = height / 2 - 5;

    /* 用逻辑坐标画圆：逐行解 x = sqrt(r^2 - y^2)，
     * 保证在逻辑空间里是正圆，再统一映射到物理坐标。 */
    const int r = 43;
    for (int dy = -r; dy <= r; dy++) {
        const int rem = r * r - dy * dy;
        if (rem < 0) continue;
        const int dx = int_sqrt(rem);
        st7306_write_point(d, MAP_X(cx - dx, cy + dy), MAP_Y(cx - dx, cy + dy), RED);
        st7306_write_point(d, MAP_X(cx + dx, cy + dy), MAP_Y(cx + dx, cy + dy), RED);
    }

    for (int i = -28; i <= 25; i++) {
        st7306_write_point(d, MAP_X(cx, cy + i), MAP_Y(cx, cy + i), BLACK);
    }
    for (int i = 0; i < 14; i++) {
        st7306_write_point(d, MAP_X(cx - i, cy - 28 + i), MAP_Y(cx - i, cy - 28 + i), BLACK);
        st7306_write_point(d, MAP_X(cx + i, cy - 28 + i), MAP_Y(cx + i, cy - 28 + i), BLACK);
    }

    /* 四角标记：左上红、右上青、左下黑、右下白（白块外描边） */
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            st7306_write_point(d, MAP_X(3 + x, 3 + y), MAP_Y(3 + x, 3 + y), RED);
            st7306_write_point(d, MAP_X(width - 11 + x, 3 + y), MAP_Y(width - 11 + x, 3 + y), CYAN);
            st7306_write_point(d, MAP_X(3 + x, height - 11 + y), MAP_Y(3 + x, height - 11 + y), BLACK);
            st7306_write_point(d, MAP_X(width - 11 + x, height - 11 + y),
                               MAP_Y(width - 11 + x, height - 11 + y), BLACK);
        }
    }

    /* 文字（逐点映射，保证跟着旋转走） */
    char buf[40];
    snprintf(buf, sizeof(buf), "ROTATION %d", angles[rotation]);
    const int tw = text_width(buf, 2);
    const int tx = (width - tw) / 2;
    for (int i = 0; buf[i]; i++) {
        const uint8_t *glyph = font5x7[(unsigned char)buf[i] - 32];
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if (glyph[col] & (1U << row)) {
                    for (int sy = 0; sy < 2; sy++) {
                        for (int sx = 0; sx < 2; sx++) {
                            const int x = tx + i * 12 + col * 2 + sx + 1;
                            const int y = 11 + row * 2 + sy;
                            st7306_write_point(d, MAP_X(x, y), MAP_Y(x, y), WHITE);
                        }
                    }
                }
            }
        }
    }

    snprintf(buf, sizeof(buf), "LOGICAL %d x %d", width, height);
    const int sw = text_width(buf, 1);
    for (int i = 0; buf[i]; i++) {
        const uint8_t *glyph = font5x7[(unsigned char)buf[i] - 32];
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if (glyph[col] & (1U << row)) {
                    const int x = (width - sw) / 2 + i * 6 + col;
                    const int y = 50 + row;
                    st7306_write_point(d, MAP_X(x, y), MAP_Y(x, y), BLACK);
                }
            }
        }
    }

    #undef MAP_X
    #undef MAP_Y
}
/* =============================================================== */
/* 以下为 4 色屏专属测试（黑白屏无对应页）                            */
/* =============================================================== */

/* --------------------------------------------------------------- */
/* 16. 四色纯色轮播（每 1.5 秒整屏换色，先确认颜色定义没错）           */
/* --------------------------------------------------------------- */

static void page_palette(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)elapsed;
    static const uint16_t colors[4] = {WHITE, CYAN, RED, BLACK};
    static const char *names[4] = {"WHITE", "CYAN", "RED", "BLACK"};
    const int step = frame / 2;  /* 4 fps -> 每 2 帧换色 */
    const uint16_t bg = colors[step & 3];
    const uint16_t fg = (bg == WHITE || bg == CYAN) ? BLACK : WHITE;

    rect(d, 0, 0, W - 1, H - 1, bg);
    text_center(d, 60, "COLOR", 6, fg);
    text_center(d, 130, "CYCLE", 6, fg);
    text_center(d, 210, names[step & 3], 8, fg);

    /* 四个小色块，随时能看到四种颜色的相对关系 */
    const int bx = 40, by = 300, bw = 50, bh = 50;
    for (int i = 0; i < 4; i++) {
        const int x = bx + i * (bw + 10);
        rect(d, x, by, x + bw, by + bh, colors[i]);
        ui_draw_rectangle(d, x, by, x + bw, by + bh, fg);
        if (i == (step & 3)) {
            ui_draw_rectangle(d, x - 3, by - 3, x + bw + 3, by + bh + 3, fg);
        }
    }
    text_center(d, 370, "BIT1=RED  BIT0=CYAN  BOTH=BLACK", 1, fg);
}

/* --------------------------------------------------------------- */
/* 17. 四色色卡 + 编码说明                                           */
/* --------------------------------------------------------------- */

static void page_color_card(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame; (void)elapsed;
    chrome(d, "COLOR CARD", "2BIT CODE / PANEL PRIMARIES");

    static const uint16_t colors[4] = {WHITE, CYAN, RED, BLACK};
    static const char *names[4] = {"WHITE", "CYAN", "RED", "BLACK"};
    static const char *codes[4] = {"00", "01", "10", "11"};

    for (int i = 0; i < 4; i++) {
        const int x = 12 + (i & 1) * 142;
        const int y = 64 + (i >> 1) * 130;
        rect(d, x, y, x + 130, y + 92, colors[i]);
        ui_draw_rectangle(d, x, y, x + 130, y + 92, BLACK);
        /* 色块上的文字用对比色 */
        const uint16_t fg = (colors[i] == WHITE || colors[i] == CYAN) ? BLACK : WHITE;
        text(d, x + 8, y + 10, names[i], 2, fg);
        text(d, x + 8, y + 34, codes[i], 3, fg);
        char buf[16];
        snprintf(buf, sizeof(buf), "= 0x%02X", (unsigned)(i));
        /* 注意：这里 i 恰好等于颜色编码 0/1/2/3 */
        text(d, x + 60, y + 44, buf, 1, fg);
    }

    text(d, 14, 322, "BIT1: RED PLANE", 2, RED);
    text(d, 14, 344, "BIT0: CYAN PLANE", 2, CYAN);
    text(d, 14, 366, "BOTH SET = BLACK", 1, BLACK);
}

/* --------------------------------------------------------------- */
/* 18. 四色分块对比（同一图形用四种颜色画）                          */
/* --------------------------------------------------------------- */

static void draw_glyph_block(st7306_handle_t d, int x0, int y0, int w, int h,
                             uint16_t bg, uint16_t fg, const char *label)
{
    rect(d, x0, y0, x0 + w - 1, y0 + h - 1, bg);
    ui_draw_rectangle(d, x0, y0, x0 + w - 1, y0 + h - 1, BLACK);

    const int cx = x0 + w / 2;
    const int cy = y0 + h / 2;
    ui_draw_filled_circle(d, cx, cy - 10, 26, fg);
    ui_draw_rectangle(d, x0 + 16, y0 + h - 52, x0 + w - 17, y0 + h - 22, fg);

    text(d, x0 + 8, y0 + 8, label, 1, fg);
    text(d, x0 + 8, y0 + h - 16, "AA BB 123", 1, fg);
}

static void page_quad(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)frame; (void)elapsed;
    chrome(d, "QUAD SPLIT", "SAME ARTWORK IN 4 COLOR PAIRS");

    const int w = 143, h = 146;
    draw_glyph_block(d, 4,   58,  w, h, WHITE, BLACK, "BLK/WHITE");
    draw_glyph_block(d, 153, 58,  w, h, WHITE, RED,   "RED/WHITE");
    draw_glyph_block(d, 4,   210, w, h, WHITE, CYAN,  "CYAN/WHITE");
    draw_glyph_block(d, 153, 210, w, h, CYAN,  RED,   "RED/CYAN");
    text_center(d, 364, "SAME SHAPE, DIFFERENT READABILITY", 1, BLACK);
}

/* --------------------------------------------------------------- */
/* 19. 彩色演示：红色警示 + 青色进度（模拟真实 HMI 场景）             */
/* --------------------------------------------------------------- */

static void page_hmi(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)elapsed;
    chrome(d, "HMI DEMO", "ALERT / STATUS / PROGRESS IN 4 COLORS");

    /* 顶部警示条：红色背景下白字 */
    rect(d, 8, 60, 291, 92, RED);
    text(d, 18, 68, "WARNING: HIGH TEMP", 2, WHITE);
    ui_draw_rectangle(d, 8, 60, 291, 92, BLACK);

    /* 三个状态指示灯 */
    static const char *labels[3] = {"POWER", "LINK", "FAULT"};
    for (int i = 0; i < 3; i++) {
        const int y = 108 + i * 34;
        text(d, 16, y + 4, labels[i], 1, BLACK);
        const bool active = (i == 0) || ((frame / (6 + i * 4)) & 1);
        uint16_t c = WHITE;
        if (i == 0) c = CYAN;
        if (i == 1) c = CYAN;
        if (i == 2) c = RED;
        if (active) ui_draw_filled_circle(d, 78, y + 6, 8, c);
        else ui_draw_circle(d, 78, y + 6, 8, BLACK);
        text(d, 94, y + 4, active ? "OK" : "--", 1, BLACK);
    }

    /* 进度条：黑底 + 青色填充 + 红色游标 */
    text(d, 16, 222, "PROGRESS", 1, BLACK);
    ui_draw_rectangle(d, 14, 240, 285, 270, BLACK);
    const int percent = wave(frame, 60) * 100 / 60;
    const int filled = 14 + (285 - 15) * percent / 100;
    rect(d, 15, 241, filled, 269, CYAN);
    vline(d, filled, 238, 272, RED);

    char buf[24];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    text(d, 240, 248, buf, 2, percent > 80 ? RED : BLACK);

    /* 底部说明 */
    hline(d, 14, 285, 292, BLACK);
    text(d, 16, 302, "RED SENDS ALARM, CYAN SHOWS NORMAL.", 1, BLACK);
    text(d, 16, 318, "THAT IS ALL A 4-COLOR RLCD NEEDS.", 1, BLACK);
    text(d, 16, 340, "REFLECTION OF THE SUN, NOT A BACKLIGHT.", 1, CYAN);
}

/* --------------------------------------------------------------- */
/* 20. 动画：四彩流动条                                              */
/* --------------------------------------------------------------- */

static void page_flow(st7306_handle_t d, int frame, int64_t elapsed)
{
    (void)elapsed;
    chrome(d, "COLOR FLOW", "SCROLLING 4-COLOR MARQUEE");

    static const uint16_t seq[4] = {WHITE, CYAN, RED, BLACK};
    const int band_h = 40;
    const int seg = 25;   /* 每段宽度，整段一次填充，避免逐列写显存 */

    /* 上半屏：4 条流动彩带 */
    for (int b = 0; b < 4; b++) {
        const int y0 = 60 + b * (band_h + 6);
        for (int x = 0; x < W; x += seg) {
            /* 每条带子错开相位，形成斜向流动 */
            const int idx = ((x / seg) + b + frame / 2) & 3;
            const int x1 = (x + seg - 1 < W - 1) ? (x + seg - 1) : (W - 1);
            rect(d, x, y0, x1, y0 + band_h - 1, seq[idx]);
        }
        ui_draw_rectangle(d, 0, y0, W - 1, y0 + band_h - 1, BLACK);
    }

    /* 下半屏：独立黑底文字块，不与彩带重叠 */
    rect(d, 12, 250, 287, 336, BLACK);
    text_center(d, 262, "4-COLOR", 4, WHITE);
    text_center(d, 300, "RLCD", 2, CYAN);
    text_center(d, 320, "SCROLLING BANDS", 1, RED);

    /* 底部四色指示点，当前激活色高亮 */
    for (int i = 0; i < 4; i++) {
        const int x = 100 + i * 28;
        const bool on = ((frame / 6) & 3) == i;
        if (on) ui_draw_filled_circle(d, x, 356, 8, seq[i]);
        else ui_draw_circle(d, x, 356, 8, BLACK);
        if (seq[i] == WHITE) ui_draw_circle(d, x, 356, 8, BLACK);
    }
}
/* --------------------------------------------------------------- */
/* 页面表与调度                                                      */
/* --------------------------------------------------------------- */

typedef void (*page_draw_t)(st7306_handle_t, int, int64_t);

typedef struct {
    const char *name;
    page_draw_t draw;
    uint32_t duration_ms;
    uint16_t target_fps;   /* 0 = 只画一帧并驻留；0xFFFF = 不限速 */
    bool color_page;       /* true = 4 色专属页，不参与黑白对齐计数 */
} demo_page_t;

static const demo_page_t pages[] = {
    /* --- 与黑白屏 15 页逐页对齐（顺序、时长、帧率一致） --- */
    {"Hello World",      page_hello,     3500,  0,          false},
    {"Color quantize",   page_gray,      6500,  0,          false},
    {"Geometry",         page_geometry,  5000,  0,          false},
    {"Pixel grid",       page_grid,      5000,  0,          false},
    {"Text",             page_text,      5500,  0,          false},
    {"Dashboard UI",     page_ui,        9000,  4,          false},
    {"Frame rate",       page_fps,       8500,  0xFFFF,     false},
    {"Fine patterns",    page_patterns,  6000,  2,          false},
    {"Bounce",           page_bounce,    9000,  20,         false},
    {"Starfield",        page_stars,     9000,  20,         false},
    {"Game of Life",     page_life,      12000, 3,          false},
    {"Uptime clock",     page_clock,     9000,  4,          false},
    {"Retro desktop",    page_retro,     6500,  0,          false},
    {"Inversion",        page_inverse,   6000,  2,          false},
    {"Rotation",         page_rotation,  12000, 4,          false},

    /* --- 4 色屏专属 --- */
    {"Color cycle",      page_palette,   6000,  4,          true},
    {"Color card",       page_color_card, 7000, 0,          true},
    {"Quad split",       page_quad,      7000,  0,          true},
    {"HMI demo",         page_hmi,       10000, 8,          true},
    {"Color flow",       page_flow,      10000, 12,         true},
};

static void page_reset(int page)
{
    switch (page) {
        case 6:  memset(&perf, 0, sizeof(perf)); break;
        case 8:  bounce_reset(); break;
        case 9:
            rng_state = 0x51a7f13du;
            for (int i = 0; i < STAR_COUNT; i++) star_reset(i, true);
            break;
        case 10: life_seed(); break;
        default: break;
    }
}

/**
 * 跑一整轮：先按黑白屏的顺序与节奏跑完 15 页，再进入 4 色专属页，
 * 然后回到第 1 页循环。这样两块屏同时上电时，前 15 页始终同步。
 */
void color4_demo_run(st7306_handle_t lcd)
{
    if (!lcd) {
        return;
    }

    g_total_pages = ARRAY_LEN(pages);
    int page = 0;

    while (1) {
        const demo_page_t *p = &pages[page];
        g_page_index = page;
        page_reset(page);
        ESP_LOGI(TAG, "Page %d/%d: %s", page + 1, ARRAY_LEN(pages), p->name);

        const int64_t started = esp_timer_get_time();
        TickType_t wake = xTaskGetTickCount();
        int frame = 0;

        while (1) {
            const int64_t draw_started = esp_timer_get_time();
            st7306_clear(lcd);
            p->draw(lcd, frame, draw_started - started);
            const esp_err_t err = st7306_display(lcd);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "display failed: %d", (int)err);
            }
            if (p->target_fps == 0xFFFF) {
                perf_record((uint32_t)(esp_timer_get_time() - draw_started));
            }

            if (p->target_fps == 0) {
                vTaskDelay(pdMS_TO_TICKS(p->duration_ms));
                break;
            }

            frame++;
            if (esp_timer_get_time() - started >= (int64_t)p->duration_ms * 1000) {
                break;
            }
            if (p->target_fps != 0xFFFF) {
                TickType_t interval = pdMS_TO_TICKS(1000 / p->target_fps);
                if (!interval) {
                    interval = 1;
                }
                vTaskDelayUntil(&wake, interval);
            }
        }

        page = (page + 1) % ARRAY_LEN(pages);
    }
}

/** 只跑 4 色专属页，用于上电后快速确认四色是否正常。 */
void color4_demo_color_pages(st7306_handle_t lcd)
{
    if (!lcd) {
        return;
    }
    g_total_pages = ARRAY_LEN(pages);

    for (int i = 0; i < ARRAY_LEN(pages); i++) {
        if (!pages[i].color_page) {
            continue;
        }
        g_page_index = i;
        page_reset(i);
        ESP_LOGI(TAG, "Color page: %s", pages[i].name);

        const int64_t started = esp_timer_get_time();
        TickType_t wake = xTaskGetTickCount();
        int frame = 0;
        while (1) {
            st7306_clear(lcd);
            pages[i].draw(lcd, frame, esp_timer_get_time() - started);
            st7306_display(lcd);

            if (pages[i].target_fps == 0) {
                vTaskDelay(pdMS_TO_TICKS(pages[i].duration_ms));
                break;
            }
            frame++;
            if (esp_timer_get_time() - started >= (int64_t)pages[i].duration_ms * 1000) {
                break;
            }
            TickType_t interval = pdMS_TO_TICKS(1000 / pages[i].target_fps);
            if (!interval) {
                interval = 1;
            }
            vTaskDelayUntil(&wake, interval);
        }
    }
}
