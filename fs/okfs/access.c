#include <fs/okfs/access.h>
#include <kernel/okfs.h>

int okfs_valid_path(const char *path) {
    if (path == 0 || path[0] != '/' || path[1] == '\0') {
        return 0;
    }

    if (path[1] == '/' || path[1] == '.') {
        return 0;
    }

    for (uint32_t index = 1; path[index] != '\0'; index++) {
        if (path[index] == '/' && path[index + 1] == '/') {
            return 0;
        }
    }

    return 1;
}

int okfs_entry_used(const struct okfs_entry *entry) {
    if (entry == 0) {
        return 0;
    }

    return (entry->flags & OKFS_FLAG_USED) != 0;
}

int okfs_find_entry(const struct okfs_entry *entries, const char *path) {
    if (entries == 0 || !okfs_valid_path(path)) {
        return -1;
    }

    for (uint32_t index = 0; index < OKFS_MAX_FILES; index++) {
        if (okfs_entry_used(&entries[index])) {
            const char *left = entries[index].name;
            const char *right = path + 1;

            while (*left != '\0' && *left == *right) {
                left++;
                right++;
            }

            if (*left == '\0' && *right == '\0') {
                return (int)index;
            }
        }
    }

    return -1;
}

int okfs_check_access(const struct okfs_entry *entry, uint32_t flags) {
    if (!okfs_entry_used(entry)) {
        return -1;
    }

    if (flags == 0) {
        return -1;
    }

    return 0;
}
