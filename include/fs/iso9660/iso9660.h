#ifndef INCLUDE_FS_ISO9660_ISO9660_H
#define INCLUDE_FS_ISO9660_ISO9660_H

#include <stdint.h>

/*
 * Read-only ISO9660 support over a primary-channel ATAPI CD-ROM device.
 *
 * iso9660_mount() probes the primary master and slave for an ATAPI device.
 * Paths are absolute and use ISO9660 primary-volume 8-bit identifiers.
 */
int iso9660_mount(void);
int iso9660_is_mounted(void);
int iso9660_read_file(const char *path, void *buffer, uint32_t capacity);
int iso9660_list_directory(const char *path, char *buffer, uint32_t capacity);

#endif
