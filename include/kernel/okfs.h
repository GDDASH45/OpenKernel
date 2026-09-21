#ifndef OKFS_H
#define OKFS_H

#include <stdint.h>

int okfs_mount(void);
int okfs_read(const char *path, void *buffer, uint32_t capacity);
int okfs_write(const char *path, const void *data, uint32_t size);
int okfs_unpack_tar(uint32_t tar_address);

#endif