
#include <write/write.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/*
 * OpenKernel E Write
 *
 * Expanded kernel output library.
 *
 * Backend:
 *     k_print(const char *str)
 *
 * Features:
 *     - String and character output
 *     - Newline output
 *     - Repeated characters
 *     - Signed and unsigned decimal
 *     - Lowercase and uppercase hexadecimal
 *     - Fixed-width hexadecimal
 *     - Binary and octal
 *     - Pointer output
 *     - Formatted output
 *     - Padding and left alignment
 *     - Boolean, status, and error output
 *
 * This implementation does not require libc printf().
 */

/* ---------------------------------------------------------
 * Internal constants
 * --------------------------------------------------------- */

static const char ewrite_digits_lower[] =
    "0123456789abcdef";

static const char ewrite_digits_upper[] =
    "0123456789ABCDEF";

/* ---------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------- */

static size_t ewrite_strlen(const char *str)
{
    size_t length = 0;

    if (str == NULL)
        return 0;

    while (str[length] != '\0')
        length++;

    return length;
}

static void ewrite_unsigned_base(
    uint64_t value,
    unsigned int base,
    int uppercase)
{
    char buffer[65];
    size_t length = 0;
    const char *digits;

    if (base < 2 || base > 16) {
        ewrite("(invalid base)");
        return;
    }

    digits = uppercase
        ? ewrite_digits_upper
        : ewrite_digits_lower;

    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    while (length > 0)
        ewrite_char(buffer[--length]);
}

static void ewrite_unsigned_width(
    uint64_t value,
    unsigned int base,
    unsigned int width,
    char padding,
    int uppercase)
{
    char buffer[65];
    size_t length = 0;
    const char *digits;

    if (base < 2 || base > 16) {
        ewrite("(invalid base)");
        return;
    }

    digits = uppercase
        ? ewrite_digits_upper
        : ewrite_digits_lower;

    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    while (length < width) {
        ewrite_char(padding);
        width--;
    }

    while (length > 0)
        ewrite_char(buffer[--length]);
}

static uint64_t ewrite_signed_magnitude(int64_t value)
{
    if (value < 0)
        return (uint64_t)(-(value + 1)) + 1;

    return (uint64_t)value;
}

static void ewrite_signed_decimal(int64_t value)
{
    if (value < 0) {
        ewrite_char('-');
        ewrite_unsigned_base(
            ewrite_signed_magnitude(value), 10, 0);
    } else {
        ewrite_unsigned_base((uint64_t)value, 10, 0);
    }
}

/*
 * Format an unsigned number into a local buffer, then print
 * it with optional width, padding, and left alignment.
 */
static void ewrite_format_number(
    uint64_t value,
    unsigned int base,
    unsigned int width,
    int left_align,
    char padding,
    int uppercase,
    int negative)
{
    char buffer[65];
    size_t length = 0;
    size_t total_length;
    size_t pad_length;
    const char *digits;

    if (base < 2 || base > 16) {
        ewrite("(invalid base)");
        return;
    }

    digits = uppercase
        ? ewrite_digits_upper
        : ewrite_digits_lower;

    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    total_length = length + (negative ? 1u : 0u);
    pad_length = width > total_length
        ? width - total_length
        : 0;

    /*
     * For zero padding, the minus sign must come first.
     * For space padding, padding precedes the sign.
     */
    if (!left_align && padding != '0')
        ewrite_repeat(padding, pad_length);

    if (negative)
        ewrite_char('-');

    if (!left_align && padding == '0')
        ewrite_repeat('0', pad_length);

    while (length > 0)
        ewrite_char(buffer[--length]);

    if (left_align)
        ewrite_repeat(' ', pad_length);
}

/* ---------------------------------------------------------
 * Basic output
 * --------------------------------------------------------- */

void ewrite(const char *str)
{
    if (str == NULL) {
        k_print("(null)");
        return;
    }

    k_print(str);
}

void ewrite_line(const char *str)
{
    ewrite(str);
    ewrite_char('\n');
}

void ewrite_char(char c)
{
    char buffer[2];

    buffer[0] = c;
    buffer[1] = '\0';

    k_print(buffer);
}

void ewrite_repeat(char c, size_t count)
{
    while (count > 0) {
        ewrite_char(c);
        count--;
    }
}

/* ---------------------------------------------------------
 * Integer output
 * --------------------------------------------------------- */

void ewrite_uint(uint64_t value)
{
    ewrite_unsigned_base(value, 10, 0);
}

void ewrite_int(int64_t value)
{
    ewrite_signed_decimal(value);
}

void ewrite_hex(uint64_t value)
{
    ewrite_unsigned_base(value, 16, 0);
}

void ewrite_hex_upper(uint64_t value)
{
    ewrite_unsigned_base(value, 16, 1);
}

void ewrite_hex32(uint32_t value)
{
    ewrite_hex_padded(value, 8);
}

void ewrite_hex64(uint64_t value)
{
    ewrite_hex_padded(value, 16);
}

void ewrite_hex_padded(uint64_t value, unsigned int digits)
{
    ewrite_unsigned_width(value, 16, digits, '0', 0);
}

void ewrite_uint_padded(uint64_t value, unsigned int width)
{
    ewrite_unsigned_width(value, 10, width, ' ', 0);
}

void ewrite_bin(uint64_t value)
{
    ewrite_unsigned_base(value, 2, 0);
}

void ewrite_octal(uint64_t value)
{
    ewrite_unsigned_base(value, 8, 0);
}

void ewrite_ptr(const void *ptr)
{
    ewrite("0x");
    ewrite_hex((uint64_t)(uintptr_t)ptr);
}

/* ---------------------------------------------------------
 * Boolean, status, and error output
 * --------------------------------------------------------- */

void ewrite_bool(int value)
{
    ewrite(value ? "true" : "false");
}

void ewrite_status(const char *name, int success)
{
    ewrite("[");
    ewrite(success ? " OK " : "FAIL");
    ewrite("] ");
    ewrite_line(name);
}

void ewrite_error(const char *message)
{
    ewrite("[ERROR] ");
    ewrite_line(message);
}

/* ---------------------------------------------------------
 * Formatted string output
 * --------------------------------------------------------- */

static void ewrite_format_string(
    const char *str,
    unsigned int width,
    int left_align,
    char padding)
{
    size_t length;

    if (str == NULL)
        str = "(null)";

    length = ewrite_strlen(str);

    if (!left_align && width > length)
        ewrite_repeat(padding, width - length);

    ewrite(str);

    if (left_align && width > length)
        ewrite_repeat(' ', width - length);
}

/*
 * Supported conversions:
 *
 * %c   character
 * %s   string
 * %d   signed decimal
 * %i   signed decimal
 * %u   unsigned decimal
 * %x   lowercase hexadecimal
 * %X   uppercase hexadecimal
 * %b   binary
 * %o   octal
 * %p   pointer
 * %%   literal percent
 *
 * Flags:
 * -    left alignment
 * 0    zero padding
 *
 * Width:
 * %8x
 * %04x
 * %-12s
 *
 * Length modifiers:
 * l, ll, z
 */

void ewritev(const char *format, va_list args)
{
    const char *p = format;

    if (format == NULL) {
        ewrite("(null format)");
        return;
    }

    while (*p != '\0') {
        unsigned int width = 0;
        int left_align = 0;
        int long_count = 0;
        int size_modifier = 0;
        char padding = ' ';
        char conversion;

        if (*p != '%') {
            ewrite_char(*p++);
            continue;
        }

        p++;

        if (*p == '%') {
            ewrite_char('%');
            p++;
            continue;
        }

        /* Flags. */
        for (;;) {
            if (*p == '-') {
                left_align = 1;
                p++;
            } else if (*p == '0') {
                padding = '0';
                p++;
            } else {
                break;
            }
        }

        /* Field width. */
        while (*p >= '0' && *p <= '9') {
            unsigned int digit =
                (unsigned int)(*p - '0');

            if (width > (UINT_MAX - digit) / 10)
                width = UINT_MAX;
            else
                width = width * 10 + digit;

            p++;
        }

        /* Length modifiers. */
        if (*p == 'l') {
            long_count = 1;
            p++;

            if (*p == 'l') {
                long_count = 2;
                p++;
            }
        } else if (*p == 'z') {
            size_modifier = 1;
            p++;
        }

        conversion = *p;

        if (conversion == '\0') {
            ewrite_char('%');
            break;
        }

        p++;

        switch (conversion) {
        case 'c': {
            char value = (char)va_arg(args, int);

            if (!left_align && width > 1)
                ewrite_repeat(padding, width - 1);

            ewrite_char(value);

            if (left_align && width > 1)
                ewrite_repeat(' ', width - 1);

            break;
        }

        case 's':
            ewrite_format_string(
                va_arg(args, const char *),
                width,
                left_align,
                padding);
            break;

        case 'd':
        case 'i': {
            int64_t value;

            if (long_count == 2)
                value = va_arg(args, long long);
            else if (long_count == 1)
                value = va_arg(args, long);
            else if (size_modifier)
                value = (int64_t)va_arg(args, ptrdiff_t);
            else
                value = va_arg(args, int);

            ewrite_format_number(
                ewrite_signed_magnitude(value),
                10,
                width,
                left_align,
                padding,
                0,
                value < 0);
            break;
        }

        case 'u':
        case 'x':
        case 'X':
        case 'b':
        case 'o': {
            uint64_t value;

            if (long_count == 2)
                value = va_arg(args, unsigned long long);
            else if (long_count == 1)
                value = va_arg(args, unsigned long);
            else if (size_modifier)
                value = (uint64_t)va_arg(args, size_t);
            else
                value = va_arg(args, unsigned int);

            if (conversion == 'u') {
                ewrite_format_number(
                    value, 10, width, left_align,
                    padding, 0, 0);
            } else if (conversion == 'x') {
                ewrite_format_number(
                    value, 16, width, left_align,
                    padding, 0, 0);
            } else if (conversion == 'X') {
                ewrite_format_number(
                    value, 16, width, left_align,
                    padding, 1, 0);
            } else if (conversion == 'b') {
                ewrite_format_number(
                    value, 2, width, left_align,
                    padding, 0, 0);
            } else {
                ewrite_format_number(
                    value, 8, width, left_align,
                    padding, 0, 0);
            }

            break;
        }

        case 'p': {
            uintptr_t value =
                (uintptr_t)va_arg(args, void *);

            ewrite("0x");

            ewrite_format_number(
                (uint64_t)value,
                16,
                width > 2 ? width - 2 : 0,
                left_align,
                padding,
                0,
                0);
            break;
        }

        default:
            ewrite_char('%');
            ewrite_char(conversion);
            break;
        }
    }
}

void ewritef(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    ewritev(format, args);
    va_end(args);
}
