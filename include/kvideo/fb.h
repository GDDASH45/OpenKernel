#ifndef FB_H
#define FB_H

#include <stdint.h>
#include <kernel/multiboot.h>

int fb_init(const struct multiboot_info *mbi);
int fb_write(const char *buf, uint32_t size);
void fb_put_pixel(int x, int y, uint8_t color);
void fb_set_pixel(int x, int y, uint8_t color);
void fb_clear(uint32_t color);
void fb_fill_rect(int x, int y, int width, int height, uint32_t color);
void fb_draw_char(int x, int y, char character, uint32_t color);
void fb_draw_text(int x, int y, const char *text, uint32_t color);
uint32_t fb_width(void);
uint32_t fb_height(void);
void set_mode_13h(void);

#endif