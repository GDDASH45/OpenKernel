#ifndef OPENKERNEL_FS_EXT4_H
#define OPENKERNEL_FS_EXT4_H

#include <stdint.h>

/*
 * Read-only ext4 access over the primary ATA device.
 *
 * partition_lba is the first sector of the ext4 filesystem. Pass zero
 * for a filesystem image without a partition table.
 *
 * The filesystem must use 512-byte ATA sectors. Unsupported incompatible
 * features cause ext4_mount() to fail.
 */
int ext4_mount(uint32_t partition_lba);
int ext4_is_mounted(void);
int ext4_read_file(const char *path, void *buffer, uint32_t capacity);
int ext4_list_directory(const char *path, char *buffer, uint32_t capacity);

#endif
