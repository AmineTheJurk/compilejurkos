#ifndef JURKOS_FRAMEBUFFER_H
#define JURKOS_FRAMEBUFFER_H

#include "types.h"

#define COLOR_BLACK       0x00000000
#define COLOR_BLUE        0x000000AA
#define COLOR_GREEN       0x0000AA00
#define COLOR_CYAN        0x0000AAAA
#define COLOR_RED         0x00AA0000
#define COLOR_MAGENTA     0x00AA00AA
#define COLOR_BROWN       0x00AA5500
#define COLOR_LIGHT_GRAY  0x00AAAAAA
#define COLOR_DARK_GRAY   0x00555555
#define COLOR_LIGHT_BLUE  0x005555FF
#define COLOR_LIGHT_GREEN 0x0055FF55
#define COLOR_LIGHT_CYAN  0x0055FFFF
#define COLOR_LIGHT_RED   0x00FF5555
#define COLOR_LIGHT_MAGENTA 0x00FF55FF
#define COLOR_YELLOW      0x00FFFF55
#define COLOR_WHITE       0x00FFFFFF

void fb_init(void *addr, uint32_t width, uint32_t height, uint32_t pitch, uint16_t bpp);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void fb_draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
void fb_print(const char *str);
void fb_println(const char *str);
void fb_print_color(const char *str, uint32_t fg, uint32_t bg);
void fb_clear(uint32_t color);
void fb_draw_box(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg_color, uint32_t border_color, const char *title, const char *text);

uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
void *fb_get_buffer(void);

#endif
