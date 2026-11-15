#ifndef INCLUDE_FS_SOKFS_SOKFS_H
#define INCLUDE_FS_SOKFS_SOKFS_H

#include <stdint.h>

#define SOKFS_SECTOR_SIZE 512U
#define SOKFS_NAME_MAX 23U
#define SOKFS_MAX_INODES 128U
#define SOKFS_MAX_SECTORS 131072U

#define SOKFS_TYPE_FILE 1U
#define SOKFS_TYPE_DIRECTORY 2U

struct sokfs_stat {
    uint32_t inode;
    uint32_t size;
    uint32_t mode;
    uint32_t type;
};

int sokfs_format(uint32_t start_lba, uint32_t sector_count);
int sokfs_mount(uint32_t start_lba);
int sokfs_is_mounted(void);
int sokfs_create(const char *path, uint32_t mode);
int sokfs_mkdir(const char *path, uint32_t mode);
int sokfs_read(const char *path, void *buffer, uint32_t capacity);
int sokfs_write(const char *path, const void *buffer, uint32_t size);
int sokfs_list(const char *path, char *buffer, uint32_t capacity);
int sokfs_stat(const char *path, struct sokfs_stat *stat);

#endif
