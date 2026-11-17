/**
 * @file st73xx_ui.h
 * @brief ST7306 4 色屏的绘图基元（矩形/线/圆/三角/多边形）。
 *
 * 函数名与黑白版 st73xx_ui.h 完全对齐，方便两个工程逐页对照。
 */
#ifndef ST73XX_UI_H
#define ST73XX_UI_H

#include "st7306_color4.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_draw_point(st7306_handle_t dev, int x, int y, uint16_t color);

void ui_draw_fast_hline(st7306_handle_t dev, int x, int y, int len, uint16_t color);
void ui_draw_fast_vline(st7306_handle_t dev, int x, int y, int len, uint16_t color);

void ui_draw_line(st7306_handle_t dev, int x1, int y1, int x2, int y2, uint16_t color);

void ui_draw_rectangle(st7306_handle_t dev, int x1, int y1, int x2, int y2, uint16_t color);
void ui_draw_filled_rectangle(st7306_handle_t dev, int x1, int y1, int x2, int y2, uint16_t color);

void ui_draw_triangle(st7306_handle_t dev, int x1, int y1, int x2, int y2,
                      int x3, int y3, uint16_t color);
void ui_draw_filled_triangle(st7306_handle_t dev, int x1, int y1, int x2, int y2,
                             int x3, int y3, uint16_t color);

void ui_draw_circle(st7306_handle_t dev, int xc, int yc, int r, uint16_t color);
void ui_draw_filled_circle(st7306_handle_t dev, int xc, int yc, int r, uint16_t color);

void ui_draw_polygon(st7306_handle_t dev, const int *points, int n, uint16_t color);
void ui_draw_filled_polygon(st7306_handle_t dev, const int *points, int n, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* ST73XX_UI_H */
