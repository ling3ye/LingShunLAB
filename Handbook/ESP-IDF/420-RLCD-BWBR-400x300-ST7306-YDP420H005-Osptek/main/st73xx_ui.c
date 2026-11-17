/**
 * @file st73xx_ui.c
 * @brief ST7306 4 色屏绘图基元实现（整数 Bresenham，全部走 fill_rect/write_point）。
 */
#include "st73xx_ui.h"

#include <stdlib.h>

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void swap_int(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

void ui_draw_point(st7306_handle_t dev, int x, int y, uint16_t color)
{
    st7306_write_point(dev, x, y, color);
}

void ui_draw_fast_hline(st7306_handle_t dev, int x, int y, int len, uint16_t color)
{
    if (len <= 0) return;
    st7306_fill_rect(dev, x, y, x + len - 1, y, color);
}

void ui_draw_fast_vline(st7306_handle_t dev, int x, int y, int len, uint16_t color)
{
    if (len <= 0) return;
    st7306_fill_rect(dev, x, y, x, y + len - 1, color);
}

void ui_draw_line(st7306_handle_t dev, int x1, int y1, int x2, int y2, uint16_t color)
{
    int dx = abs(x2 - x1);
    int dy = -abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        st7306_write_point(dev, x1, y1, color);
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void ui_draw_rectangle(st7306_handle_t dev, int x1, int y1, int x2, int y2, uint16_t color)
{
    if (x1 > x2) swap_int(&x1, &x2);
    if (y1 > y2) swap_int(&y1, &y2);
    st7306_fill_rect(dev, x1, y1, x2, y1, color);
    st7306_fill_rect(dev, x1, y2, x2, y2, color);
    st7306_fill_rect(dev, x1, y1, x1, y2, color);
    st7306_fill_rect(dev, x2, y1, x2, y2, color);
}

void ui_draw_filled_rectangle(st7306_handle_t dev, int x1, int y1, int x2, int y2, uint16_t color)
{
    st7306_fill_rect(dev, x1, y1, x2, y2, color);
}

void ui_draw_triangle(st7306_handle_t dev, int x1, int y1, int x2, int y2,
                      int x3, int y3, uint16_t color)
{
    ui_draw_line(dev, x1, y1, x2, y2, color);
    ui_draw_line(dev, x2, y2, x3, y3, color);
    ui_draw_line(dev, x3, y3, x1, y1, color);
}

void ui_draw_filled_triangle(st7306_handle_t dev, int x1, int y1, int x2, int y2,
                             int x3, int y3, uint16_t color)
{
    /* 扫描线填充，按 y 逐行求左右交点 */
    if (y1 > y2) { swap_int(&y1, &y2); swap_int(&x1, &x2); }
    if (y2 > y3) { swap_int(&y2, &y3); swap_int(&x2, &x3); }
    if (y1 > y2) { swap_int(&y1, &y2); swap_int(&x1, &x2); }

    const int total_height = y3 - y1;
    if (total_height <= 0) {
        ui_draw_line(dev, x1, y1, x2, y2, color);
        return;
    }

    for (int i = 0; i < total_height; i++) {
        const bool second_half = i > y2 - y1 || y2 == y1;
        const int segment_height = second_half ? (y3 - y2) : (y2 - y1);
        if (segment_height <= 0) {
            continue;
        }
        const float alpha = (float)i / (float)total_height;
        const float beta = (float)(i - (second_half ? y2 - y1 : 0)) / (float)segment_height;
        int ax = x1 + (int)((x3 - x1) * alpha);
        int bx = second_half ? (x2 + (int)((x3 - x2) * beta)) : (x1 + (int)((x2 - x1) * beta));
        if (ax > bx) swap_int(&ax, &bx);
        st7306_fill_rect(dev, ax, y1 + i, bx, y1 + i, color);
    }
}

void ui_draw_circle(st7306_handle_t dev, int xc, int yc, int r, uint16_t color)
{
    if (r <= 0) {
        st7306_write_point(dev, xc, yc, color);
        return;
    }
    int x = r;
    int y = 0;
    int err = 1 - r;
    while (x >= y) {
        st7306_write_point(dev, xc + x, yc + y, color);
        st7306_write_point(dev, xc + y, yc + x, color);
        st7306_write_point(dev, xc - y, yc + x, color);
        st7306_write_point(dev, xc - x, yc + y, color);
        st7306_write_point(dev, xc - x, yc - y, color);
        st7306_write_point(dev, xc - y, yc - x, color);
        st7306_write_point(dev, xc + y, yc - x, color);
        st7306_write_point(dev, xc + x, yc - y, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void ui_draw_filled_circle(st7306_handle_t dev, int xc, int yc, int r, uint16_t color)
{
    if (r <= 0) {
        st7306_write_point(dev, xc, yc, color);
        return;
    }
    int x = r;
    int y = 0;
    int err = 1 - r;
    while (x >= y) {
        st7306_fill_rect(dev, xc - x, yc + y, xc + x, yc + y, color);
        st7306_fill_rect(dev, xc - y, yc + x, xc + y, yc + x, color);
        st7306_fill_rect(dev, xc - x, yc - y, xc + x, yc - y, color);
        st7306_fill_rect(dev, xc - y, yc - x, xc + y, yc - x, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void ui_draw_polygon(st7306_handle_t dev, const int *points, int n, uint16_t color)
{
    if (!points || n < 2) return;
    for (int i = 0; i < n; i++) {
        const int j = (i + 1) % n;
        ui_draw_line(dev, points[i * 2], points[i * 2 + 1],
                     points[j * 2], points[j * 2 + 1], color);
    }
}

void ui_draw_filled_polygon(st7306_handle_t dev, const int *points, int n, uint16_t color)
{
    if (!points || n < 3) return;

    int min_y = points[1];
    int max_y = points[1];
    for (int i = 1; i < n; i++) {
        const int y = points[i * 2 + 1];
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }

    for (int y = min_y; y <= max_y; y++) {
        int left = 0;
        int right = 0;
        bool have = false;
        for (int i = 0; i < n; i++) {
            const int j = (i + 1) % n;
            const int y1 = points[i * 2 + 1];
            const int y2 = points[j * 2 + 1];
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                const int x1 = points[i * 2];
                const int x2 = points[j * 2];
                const int x = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
                if (!have) {
                    left = x;
                    right = x;
                    have = true;
                } else {
                    if (x < left) left = x;
                    if (x > right) right = x;
                }
            }
        }
        if (have) {
            st7306_fill_rect(dev, clampi(left, 0, ST7306_LCD_WIDTH - 1), y,
                             clampi(right, 0, ST7306_LCD_WIDTH - 1), y, color);
        }
    }
}
