#ifndef FBCON_H
#define FBCON_H

#include <stdint.h>

int fbcon_init(void);
int fbcon_is_active(void);
void fbcon_clear(void);
void fbcon_put_char(char character);
int fbcon_write(const char *buffer, uint32_t size);

#endif
