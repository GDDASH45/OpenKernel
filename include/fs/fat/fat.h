#ifndef INCLUDE_FS_FAT_FAT_H
#define INCLUDE_FS_FAT_FAT_H

#include <stdint.h>

/*
 * FAT12, FAT16, and FAT32 support over the primary ATA device.
 *
 * partition_lba is the first sector of the FAT volume. Pass zero for a
 * superfloppy image or the partition start LBA for an MBR-partitioned disk.
 *
 * File writes overwrite existing files without changing their size.
 * Directory creation, deletion, and file resizing are not implemented.
 */
int fat_mount(uint32_t partition_lba);
int fat_is_mounted(void);
int fat_read_file(const char *path, void *buffer, uint32_t capacity);
int fat_write_file(const char *path, const void *buffer, uint32_t size);
int fat_list_directory(const char *path, char *buffer, uint32_t capacity);

#endif
