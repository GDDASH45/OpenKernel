#ifndef WRITE_H
#define WRITE_H

#include <stdint.h>

#define VGA_ADDRESS 0xB8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0F

void k_clear_screen(void);
void k_print_char(char c);
void k_print(const char *str);
int kmsg_read(char *buffer, uint32_t size);
void k_set_quiet(int quiet);
void printk(const char *format, ...);

#endif