#ifndef OPENKERNEL_EWRITE_MAIN_H
#define OPENKERNEL_EWRITE_MAIN_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

void k_print(const char *str);

void ewrite(const char *str);
void ewrite_line(const char *str);
void ewrite_char(char c);
void ewrite_repeat(char c, size_t count);

void ewrite_uint(uint64_t value);
void ewrite_int(int64_t value);

void ewrite_hex(uint64_t value);
void ewrite_hex_upper(uint64_t value);
void ewrite_hex32(uint32_t value);
void ewrite_hex64(uint64_t value);
void ewrite_hex_padded(uint64_t value, unsigned int digits);

void ewrite_uint_padded(uint64_t value, unsigned int width);

void ewrite_bin(uint64_t value);
void ewrite_octal(uint64_t value);
void ewrite_ptr(const void *ptr);

void ewritef(const char *format, ...);
void ewritev(const char *format, va_list args);

void ewrite_bool(int value);
void ewrite_status(const char *name, int success);
void ewrite_error(const char *message);

#endif