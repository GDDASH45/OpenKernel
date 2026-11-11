#include <kernel/string.h>

/*
 * OpenKernel string and memory utilities.
 *
 * This file implements basic string and memory operations
 * without relying on the host C library.
 *
 * These functions are intended for freestanding kernel use.
 *
 * Strings must be null-terminated unless a function explicitly
 * accepts a fixed-size memory region.
 */

/*
 * strlen()
 *
 * Count characters until the null terminator is reached.
 *
 * The null terminator itself is not included in the result.
 */
size_t strlen(const char *str)
{
    size_t length = 0;

    /* Reject a NULL pointer. */
    if (str == 0)
        return 0;

    /* Continue until the end of the string. */
    while (str[length] != '\0')
        length++;

    return length;
}

/*
 * strcmp()
 *
 * Compare two strings lexicographically.
 *
 * Returns:
 *     Negative value if str1 is smaller.
 *     Zero if both strings are equal.
 *     Positive value if str1 is greater.
 */
int strcmp(const char *str1, const char *str2)
{
    /* Reject invalid pointers. */
    if (str1 == 0 || str2 == 0)
        return (str1 != str2) ? (str1 ? 1 : -1) : 0;

    /* Compare characters until they differ or reach the terminator. */
    while (*str1 && (*str1 == *str2))
    {
        str1++;
        str2++;
    }

    /* Return the difference between the first differing characters. */
    return (unsigned char)*str1 - (unsigned char)*str2;
}

/*
 * strncmp()
 *
 * Compare at most count characters.
 *
 * Unlike strcmp(), this function never examines more than
 * the requested number of characters.
 */
int strncmp(const char *str1, const char *str2, size_t count)
{
    /* Comparing zero characters always produces equality. */
    if (count == 0)
        return 0;

    /* Reject invalid pointers. */
    if (str1 == 0 || str2 == 0)
        return (str1 != str2) ? (str1 ? 1 : -1) : 0;

    while (count > 0)
    {
        unsigned char c1 = (unsigned char)*str1;
        unsigned char c2 = (unsigned char)*str2;

        /* Stop when the characters differ. */
        if (c1 != c2)
            return c1 - c2;

        /* Both strings have ended. */
        if (c1 == '\0')
            return 0;

        str1++;
        str2++;
        count--;
    }

    return 0;
}

/*
 * strcpy()
 *
 * Copy src into dest, including the terminating null byte.
 *
 * The destination must have enough space for the complete string.
 */
char *strcpy(char *dest, const char *src)
{
    char *original = dest;

    if (dest == 0 || src == 0)
        return original;

    while ((*dest++ = *src++) != '\0')
        ;

    return original;
}

/*
 * strncpy()
 *
 * Copy at most count characters.
 *
 * If src is shorter than count, the remaining destination
 * bytes are filled with null bytes.
 *
 * If src is at least count characters long, the result is
 * not guaranteed to be null-terminated.
 */
char *strncpy(char *dest, const char *src, size_t count)
{
    char *original = dest;
    size_t index = 0;

    if (dest == 0 || src == 0)
        return original;

    /* Copy characters from the source string. */
    while (index < count && src[index] != '\0')
    {
        dest[index] = src[index];
        index++;
    }

    /* Pad the remaining destination bytes with null bytes. */
    while (index < count)
    {
        dest[index] = '\0';
        index++;
    }

    return original;
}

/*
 * strcat()
 *
 * Append src to the end of dest.
 *
 * The destination must have enough free space for both strings.
 */
char *strcat(char *dest, const char *src)
{
    char *original = dest;

    if (dest == 0 || src == 0)
        return original;

    /* Locate the terminating null byte in dest. */
    while (*dest != '\0')
        dest++;

    /* Append the source string, including its terminator. */
    while ((*dest++ = *src++) != '\0')
        ;

    return original;
}

/*
 * strchr()
 *
 * Find the first occurrence of character in str.
 *
 * The terminating null byte is also considered searchable.
 */
char *strchr(const char *str, int character)
{
    unsigned char target = (unsigned char)character;

    if (str == 0)
        return 0;

    while (*str != '\0')
    {
        if ((unsigned char)*str == target)
            return (char *)str;

        str++;
    }

    /* Check whether the requested character is the terminator. */
    if (target == '\0')
        return (char *)str;

    return 0;
}

/*
 * strrchr()
 *
 * Find the last occurrence of character in str.
 *
 * The terminating null byte is also considered searchable.
 */
char *strrchr(const char *str, int character)
{
    const char *last = 0;
    unsigned char target = (unsigned char)character;

    if (str == 0)
        return 0;

    while (1)
    {
        if ((unsigned char)*str == target)
            last = str;

        if (*str == '\0')
            break;

        str++;
    }

    return (char *)last;
}

/*
 * strstr()
 *
 * Find the first occurrence of needle inside haystack.
 *
 * An empty needle matches at the beginning of haystack.
 */
char *strstr(const char *haystack, const char *needle)
{
    if (haystack == 0 || needle == 0)
        return 0;

    /* An empty substring matches immediately. */
    if (*needle == '\0')
        return (char *)haystack;

    while (*haystack != '\0')
    {
        const char *left = haystack;
        const char *right = needle;

        /* Compare characters at the current candidate position. */
        while (*left != '\0' &&
               *right != '\0' &&
               *left == *right)
        {
            left++;
            right++;
        }

        /* The entire needle matched. */
        if (*right == '\0')
            return (char *)haystack;

        haystack++;
    }

    return 0;
}

/*
 * memset()
 *
 * Fill count bytes of memory with the requested byte value.
 *
 * The value is converted to an unsigned byte before storage.
 */
void *memset(void *dest, int value, size_t count)
{
    unsigned char *bytes = (unsigned char *)dest;
    unsigned char byte = (unsigned char)value;

    if (dest == 0)
        return 0;

    for (size_t index = 0; index < count; index++)
        bytes[index] = byte;

    return dest;
}

/*
 * memcpy()
 *
 * Copy count bytes from src to dest.
 *
 * The source and destination must not overlap.
 * Use memmove() when overlapping memory regions are possible.
 */
void *memcpy(void *dest, const void *src, size_t count)
{
    unsigned char *destination = (unsigned char *)dest;
    const unsigned char *source = (const unsigned char *)src;

    if (dest == 0 || src == 0)
        return dest;

    for (size_t index = 0; index < count; index++)
        destination[index] = source[index];

    return dest;
}

/*
 * memcmp()
 *
 * Compare two memory regions byte by byte.
 *
 * Returns zero if they are identical.
 */
int memcmp(const void *ptr1, const void *ptr2, size_t count)
{
    const unsigned char *bytes1 = (const unsigned char *)ptr1;
    const unsigned char *bytes2 = (const unsigned char *)ptr2;

    if (ptr1 == 0 || ptr2 == 0)
        return (ptr1 != ptr2) ? (ptr1 ? 1 : -1) : 0;

    for (size_t index = 0; index < count; index++)
    {
        if (bytes1[index] != bytes2[index])
            return bytes1[index] - bytes2[index];
    }

    return 0;
}

/*
 * memmove()
 *
 * Copy count bytes while supporting overlapping memory regions.
 *
 * When the destination starts after the source and overlaps it,
 * bytes must be copied backwards to avoid overwriting unread data.
 *
 * Otherwise, copying forwards is safe.
 */
void *memmove(void *dest, const void *src, size_t count)
{
    unsigned char *destination = (unsigned char *)dest;
    const unsigned char *source = (const unsigned char *)src;

    if (dest == 0 || src == 0)
        return dest;

    /* No work is required when both pointers are identical. */
    if (dest == src || count == 0)
        return dest;

    if (destination < source)
    {
        /* Forward copy: destination is before source. */
        for (size_t index = 0; index < count; index++)
            destination[index] = source[index];
    }
    else
    {
        /* Backward copy: destination is after source. */
        for (size_t index = count; index > 0; index--)
            destination[index - 1] = source[index - 1];
    }

    return dest;
}