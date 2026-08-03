/**
 * Minimal drawing helpers that write directly into the RGB565 frame buffer
 * returned by esp_lcd_rgb_panel_get_frame_buffer(). The RGB peripheral DMA
 * continuously scans this buffer out, so anything we write appears on screen.
 */
#pragma once

#include <stdint.h>

/* Bind the drawing helpers to a frame buffer of the given geometry. */
void lcd_draw_init(uint16_t *fb, int width, int height);

/* Fill a single pixel (clipped). */
void lcd_draw_pixel(int x, int y, uint16_t color);

/* Fill an axis-aligned rectangle (clipped). */
void lcd_fill_rect(int x, int y, int w, int h, uint16_t color);

/* Fill the whole screen. */
void lcd_fill(uint16_t color);

/* Width in pixels of a string at the given integer scale. */
int lcd_text_width(const char *str, int scale);

/* Draw a NUL-terminated string at (x,y). Each font pixel becomes a
 * scale x scale block. Off-pixels are transparent (text overlays bg). */
void lcd_draw_text(int x, int y, const char *str, int scale, uint16_t color);
