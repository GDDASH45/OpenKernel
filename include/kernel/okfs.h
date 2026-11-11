
#ifndef OKFS_H
#define OKFS_H

#include <stdint.h>

#define OKFS_MAGIC "OKFS001"
#define OKFS_MAGIC_SIZE 8

#define OKFS_SUPER_LBA 2048
#define OKFS_DIRECTORY_LBA 2049
#define OKFS_DIRECTORY_SECTORS 4
#define OKFS_DATA_LBA (OKFS_DIRECTORY_LBA + OKFS_DIRECTORY_SECTORS)

#define OKFS_MAX_FILES 64
#define OKFS_NAME_SIZE 48

#define OKFS_FLAG_USED 1

struct okfs_entry {
    char name[OKFS_NAME_SIZE];
    uint32_t size;
    uint32_t start_lba;
    uint32_t sectors;
    uint32_t flags;
} __attribute__((packed));

int okfs_mount(void);
int okfs_read(const char *path, void *buffer, uint32_t capacity);
int okfs_write(const char *path, const void *data, uint32_t size);
int okfs_unpack_tar(uint32_t tar_address);

#endif
