/**
 * Frame-buffer drawing helpers - see lcd_draw.h.
 */
#include "lcd_draw.h"
#include "font8x16.h"

static uint16_t *s_fb;
static int s_w;
static int s_h;

void lcd_draw_init(uint16_t *fb, int width, int height)
{
    s_fb = fb;
    s_w  = width;
    s_h  = height;
}

void lcd_draw_pixel(int x, int y, uint16_t color)
{
    if ((unsigned)x >= (unsigned)s_w || (unsigned)y >= (unsigned)s_h) {
        return; /* clipped */
    }
    s_fb[y * s_w + x] = color;
}

void lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            lcd_draw_pixel(x + i, y + j, color);
        }
    }
}

void lcd_fill(uint16_t color)
{
    /* Fast path: the whole buffer is contiguous. */
    const int total = s_w * s_h;
    for (int i = 0; i < total; i++) {
        s_fb[i] = color;
    }
}

int lcd_text_width(const char *str, int scale)
{
    int n = 0;
    for (const char *p = str; *p; p++) {
        n++;
    }
    return n * FONT_W * scale;
}

void lcd_draw_text(int x, int y, const char *str, int scale, uint16_t color)
{
    int cx = x;
    for (const char *p = str; *p; p++) {
        const uint8_t *glyph = font8x16_glyph((unsigned char)*p);
        for (int row = 0; row < FONT_H; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < FONT_W; col++) {
                if (bits & (0x80 >> col)) {
                    lcd_fill_rect(cx + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        cx += FONT_W * scale;
    }
}
