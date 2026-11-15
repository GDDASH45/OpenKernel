#ifndef INCLUDE_FS_ISO9660_ISO9660_H
#define INCLUDE_FS_ISO9660_ISO9660_H

#include <stdint.h>

/*
 * ISO9660 support.
 *
 * iso9660_mount() probes IDE ATAPI CD-ROM devices and mounts read-only.
 * iso9660_mount_image() mounts an ISO image stored on the primary ATA disk.
 * Paths are absolute and use UTF-8 names; Joliet volumes are preferred.
 *
 * ATA-backed images support overwriting existing files at their original
 * size. ISO directory metadata, file creation, deletion, and resizing remain
 * unsupported; ATAPI media are always read-only.
 */
int iso9660_mount(void);
int iso9660_mount_image(uint32_t image_start_lba);
int iso9660_is_mounted(void);
int iso9660_read_file(const char *path, void *buffer, uint32_t capacity);
int iso9660_write_file(const char *path, const void *buffer, uint32_t size);
int iso9660_list_directory(const char *path, char *buffer, uint32_t capacity);

#endif
