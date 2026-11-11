#ifndef _KERNEL_STRING_H
#define _KERNEL_STRING_H

#include <stdint.h>
#include <stddef.h>

/*
 * Return the length of a null-terminated string.
 */
size_t strlen(const char *str);

/*
 * Compare two null-terminated strings.
 */
int strcmp(const char *str1, const char *str2);

/*
 * Compare at most count characters.
 */
int strncmp(const char *str1, const char *str2, size_t count);

/*
 * Copy a null-terminated string.
 */
char *strcpy(char *dest, const char *src);

/*
 * Copy at most count characters.
 */
char *strncpy(char *dest, const char *src, size_t count);

/*
 * Concatenate two strings.
 */
char *strcat(char *dest, const char *src);

/*
 * Find the first occurrence of a character.
 */
char *strchr(const char *str, int character);

/*
 * Find the last occurrence of a character.
 */
char *strrchr(const char *str, int character);

/*
 * Find the first occurrence of a substring.
 */
char *strstr(const char *haystack, const char *needle);

/*
 * Fill a string buffer with a character.
 */
void *memset(void *dest, int value, size_t count);

/*
 * Copy a block of memory.
 */
void *memcpy(void *dest, const void *src, size_t count);

/*
 * Compare two blocks of memory.
 */
int memcmp(const void *ptr1, const void *ptr2, size_t count);

/*
 * Move a block of memory safely, including overlapping regions.
 */
void *memmove(void *dest, const void *src, size_t count);

#endif