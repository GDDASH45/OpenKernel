#ifndef USER_STDIO_H
#define USER_STDIO_H

#include <stdint.h>
#include <user/syscall.h>

static void pr_error_write_string(const char *text)
{
    if (text == 0) {
        text = "(null)";
    }

    while (*text != '\0') {
        (void)ok_putchar(*text);
        text++;
    }
}

static void pr_error_write_unsigned(uint32_t value, uint32_t base)
{
    static const char digits[] = "0123456789abcdef";
    char buffer[10];
    uint32_t length = 0;

    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    while (length != 0) {
        (void)ok_putchar(buffer[--length]);
    }
}

static void pr_error_write_signed(int32_t value)
{
    if (value < 0) {
        (void)ok_putchar('-');
        pr_error_write_unsigned((uint32_t)(-(value + 1)) + 1, 10);
        return;
    }

    pr_error_write_unsigned((uint32_t)value, 10);
}

static void pr_error(const char *format, ...)
{
    __builtin_va_list arguments;

    (void)ok_console_write("error: ", 7);

    if (format == 0) {
        (void)ok_console_write("(null)\n", 7);
        return;
    }

    __builtin_va_start(arguments, format);

    while (*format != '\0') {
        if (*format != '%') {
            (void)ok_putchar(*format);
            format++;
            continue;
        }

        format++;

        if (*format == '\0') {
            (void)ok_putchar('%');
            break;
        }

        switch (*format) {
        case '%':
            (void)ok_putchar('%');
            break;

        case 'c':
            (void)ok_putchar((char)__builtin_va_arg(arguments, int));
            break;

        case 's':
            pr_error_write_string(
                __builtin_va_arg(arguments, const char *)
            );
            break;

        case 'd':
        case 'i':
            pr_error_write_signed(
                __builtin_va_arg(arguments, int32_t)
            );
            break;

        case 'u':
            pr_error_write_unsigned(
                __builtin_va_arg(arguments, uint32_t),
                10
            );
            break;

        case 'x':
            pr_error_write_unsigned(
                __builtin_va_arg(arguments, uint32_t),
                16
            );
            break;

        default:
            (void)ok_putchar('%');
            (void)ok_putchar(*format);
            break;
        }

        format++;
    }

    __builtin_va_end(arguments);

    (void)ok_putchar('\n');
}

#endif