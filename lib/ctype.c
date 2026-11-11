#include <kernel/ctype.h>

/*
 * OpenKernel Character Classification
 *
 * This file implements character classification and conversion
 * functions for ASCII characters.
 *
 * Supported character range:
 *     0x00 - 0x7F
 *
 * Characters outside the ASCII range are not classified.
 * Conversion functions leave unsupported values unchanged.
 *
 * No locale support is provided.
 */

/*
 * isascii()
 *
 * Check whether a character belongs to the ASCII range.
 *
 * Returns:
 *     Nonzero if c is between 0 and 127.
 *     Zero otherwise.
 */
int isascii(int c)
{
    return (c >= 0 && c <= 0x7F);
}

/*
 * isalpha()
 *
 * Check whether a character is an alphabetic ASCII character.
 *
 * Accepted ranges:
 *     A-Z
 *     a-z
 */
int isalpha(int c)
{
    return ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z'));
}

/*
 * isupper()
 *
 * Check whether a character is an uppercase ASCII letter.
 */
int isupper(int c)
{
    return (c >= 'A' && c <= 'Z');
}

/*
 * islower()
 *
 * Check whether a character is a lowercase ASCII letter.
 */
int islower(int c)
{
    return (c >= 'a' && c <= 'z');
}

/*
 * isdigit()
 *
 * Check whether a character is a decimal digit.
 *
 * Accepted range:
 *     0-9
 */
int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

/*
 * isalnum()
 *
 * Check whether a character is alphabetic or numeric.
 */
int isalnum(int c)
{
    return (isalpha(c) || isdigit(c));
}

/*
 * isxdigit()
 *
 * Check whether a character is a hexadecimal digit.
 *
 * Accepted ranges:
 *     0-9
 *     A-F
 *     a-f
 */
int isxdigit(int c)
{
    return (isdigit(c) ||
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f'));
}

/*
 * isspace()
 *
 * Check whether a character is an ASCII whitespace character.
 *
 * Accepted characters:
 *     Space
 *     Horizontal tab
 *     Line feed
 *     Vertical tab
 *     Form feed
 *     Carriage return
 */
int isspace(int c)
{
    return (c == ' '  ||
            c == '\t' ||
            c == '\n' ||
            c == '\v' ||
            c == '\f' ||
            c == '\r');
}

/*
 * isblank()
 *
 * Check whether a character is a horizontal whitespace character.
 *
 * Accepted characters:
 *     Space
 *     Horizontal tab
 */
int isblank(int c)
{
    return (c == ' ' || c == '\t');
}

/*
 * iscntrl()
 *
 * Check whether a character is an ASCII control character.
 *
 * Accepted ranges:
 *     0x00 - 0x1F
 *     0x7F
 */
int iscntrl(int c)
{
    return ((c >= 0x00 && c <= 0x1F) ||
            c == 0x7F);
}

/*
 * isprint()
 *
 * Check whether a character is printable.
 *
 * Printable ASCII includes:
 *     0x20 - 0x7E
 */
int isprint(int c)
{
    return (c >= 0x20 && c <= 0x7E);
}

/*
 * isgraph()
 *
 * Check whether a character is printable and not a space.
 *
 * Printable graphical ASCII includes:
 *     0x21 - 0x7E
 */
int isgraph(int c)
{
    return (c >= 0x21 && c <= 0x7E);
}

/*
 * ispunct()
 *
 * Check whether a character is a printable punctuation character.
 *
 * Letters, digits, and whitespace are excluded.
 */
int ispunct(int c)
{
    return (isgraph(c) && !isalnum(c));
}

/*
 * tolower()
 *
 * Convert an uppercase ASCII letter to lowercase.
 *
 * Characters outside A-Z are returned unchanged.
 */
int tolower(int c)
{
    if (isupper(c))
        return c + ('a' - 'A');

    return c;
}

/*
 * toupper()
 *
 * Convert a lowercase ASCII letter to uppercase.
 *
 * Characters outside a-z are returned unchanged.
 */
int toupper(int c)
{
    if (islower(c))
        return c - ('a' - 'A');

    return c;
}