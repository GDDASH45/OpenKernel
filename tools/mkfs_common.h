#ifndef OPENKERNEL_MKFS_COMMON_H
#define OPENKERNEL_MKFS_COMMON_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MKFS_SECTOR_SIZE 512U

static int mkfs_parse_u32(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || text[0] == '\0' || *end != '\0' ||
        parsed > UINT32_MAX)
        return -1;

    *value = (uint32_t)parsed;
    return 0;
}

static int mkfs_open_image(const char *path, FILE **image, uint64_t *size)
{
    long end;

    *image = fopen(path, "r+b");
    if (*image == NULL) {
        perror(path);
        return -1;
    }

    if (fseek(*image, 0, SEEK_END) != 0 ||
        (end = ftell(*image)) < 0) {
        perror("image size");
        fclose(*image);
        return -1;
    }

    *size = (uint64_t)end;
    return 0;
}

static int mkfs_write_at(FILE *image, uint64_t offset,
                         const void *buffer, size_t size)
{
    if (offset > LONG_MAX || fseek(image, (long)offset, SEEK_SET) != 0 ||
        fwrite(buffer, 1, size, image) != size) {
        perror("image write");
        return -1;
    }

    return 0;
}

static int mkfs_zero_at(FILE *image, uint64_t offset, uint64_t size)
{
    static const uint8_t zero[4096];

    if (offset > LONG_MAX || fseek(image, (long)offset, SEEK_SET) != 0) {
        perror("image seek");
        return -1;
    }

    while (size != 0) {
        size_t amount = size > sizeof(zero) ? sizeof(zero) : (size_t)size;
        if (fwrite(zero, 1, amount, image) != amount) {
            perror("image write");
            return -1;
        }
        size -= amount;
    }

    return 0;
}

static int mkfs_finish(FILE *image)
{
    if (fflush(image) != 0) {
        perror("image flush");
        fclose(image);
        return -1;
    }

    if (fclose(image) != 0) {
        perror("image close");
        return -1;
    }

    return 0;
}

#endif
