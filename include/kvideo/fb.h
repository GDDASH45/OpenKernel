#ifndef FB_H
#define FB_H

#include <stdint.h>

void fb_init(void);
int fb_write(const char *buf, uint32_t size);
void fb_put_pixel(int x, int y, uint8_t color);
void fb_set_pixel(int x, int y, uint8_t color);
void set_mode_13h(void);

#endif