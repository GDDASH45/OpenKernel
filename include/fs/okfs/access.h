#ifndef _INCLUDE_FS_OKFS_ACCESS_H
#define _INCLUDE_FS_OKFS_ACCESS_H

#include <stdint.h>

struct okfs_entry;

int okfs_valid_path(const char *path);
int okfs_entry_used(const struct okfs_entry *entry);
int okfs_find_entry(const struct okfs_entry *entries, const char *path);
int okfs_check_access(const struct okfs_entry *entry, uint32_t flags);

#endif