#ifndef INCLUDE_FS_OKFS_FILEOPS_H
#define INCLUDE_FS_OKFS_FILEOPS_H

#include <fs/okfs/okfs.h>
#include <stdint.h>

int okfs_fileops_read(const char *path, void *buffer, uint32_t capacity);
int okfs_fileops_write(const char *path, const void *data, uint32_t size);
int okfs_fileops_create(const char *path, uint32_t mode);
int okfs_fileops_mkdir(const char *path, uint32_t mode);
int okfs_fileops_chmod(const char *path, uint32_t mode);
int okfs_fileops_stat(const char *path, struct okfs_stat *stat);
int okfs_fileops_unpack_tar(uint32_t tar_address);

#endif
