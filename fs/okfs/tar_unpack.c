#include <kernel/tar.h>
#include <fs/okfs/fileops.h>
#include <fs/okfs/okfs2_internal.h>

static int tar_octal(const char *field, uint32_t length, uint32_t *value) {
    uint32_t parsed = 0;
    uint32_t digits = 0;

    for (uint32_t index = 0; index < length; index++) {
        if (field[index] < '0' || field[index] > '7') {
            break;
        }
        {
            uint32_t digit = (uint32_t)(field[index] - '0');
            if (parsed > (0xFFFFFFFFU - digit) / 8U) {
                return -1;
            }
            parsed = parsed * 8U + digit;
        }
        digits++;
    }

    *value = digits == 0 ? 0 : parsed;
    return 0;
}

static uint32_t tar_string_length(const char *value, uint32_t maximum) {
    uint32_t length = 0;

    while (length < maximum && value[length] != '\0') {
        length++;
    }
    return length;
}

static int create_parent_directories(const char *path) {
    char parent[OKFS_PATH_MAX + 1];
    uint32_t length = 0;

    while (path[length] != '\0') {
        if (++length > OKFS_PATH_MAX) {
            return -1;
        }
    }

    for (uint32_t index = 1; index < length; index++) {
        if (path[index] == '/') {
            for (uint32_t prefix = 0; prefix < index; prefix++) {
                parent[prefix] = path[prefix];
            }
            parent[index] = '\0';
            if (okfs_fileops_mkdir(parent, 0755) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

static int is_boot_path(const char *path, uint32_t length) {
    return length >= 5 && path[0] == '/' && path[1] == 'b' &&
           path[2] == 'o' && path[3] == 'o' && path[4] == 't' &&
           (length == 5 || path[5] == '/');
}

int okfs_fileops_unpack_tar(uint32_t tar_address) {
    struct tar_header *header =
        (struct tar_header *)(uintptr_t)tar_address;
    uint32_t unpacked = 0;

    while (header->name[0] != '\0') {
        uint32_t size;
        uint32_t mode;
        uint32_t name_length =
            tar_string_length(header->name, sizeof(header->name));
        uint32_t prefix_length =
            tar_string_length(header->prefix, sizeof(header->prefix));
        uint32_t path_length = 1;
        char path[OKFS_PATH_MAX + 1];
        uint32_t prefix_start = 0;

        if (tar_octal(header->size, sizeof(header->size), &size) != 0 ||
            tar_octal(header->mode, sizeof(header->mode), &mode) != 0) {
            return -1;
        }
        if (size > 0xFFFFFFFFU - 1023U ||
            tar_address > 0xFFFFFFFFU - 512U -
                ((size + 511U) / 512U) * 512U) {
            return -1;
        }
        if (name_length >= 2 && header->name[0] == '.' &&
            header->name[1] == '/') {
            name_length -= 2;
            prefix_start = 2;
        }

        path[0] = '/';
        if (prefix_length != 0) {
            if (prefix_length + 1 >= sizeof(path)) {
                return -1;
            }
            for (uint32_t index = 0; index < prefix_length; index++) {
                path[path_length++] = header->prefix[index];
            }
            if (name_length != 0) {
                if (path_length >= OKFS_PATH_MAX) {
                    return -1;
                }
                path[path_length++] = '/';
            }
        }
        if (name_length > OKFS_PATH_MAX - path_length) {
            return -1;
        }
        for (uint32_t index = 0; index < name_length; index++) {
            path[path_length++] = header->name[prefix_start + index];
        }
        if (path_length > 1 && path[path_length - 1] == '/') {
            path_length--;
        }
        path[path_length] = '\0';

        if (path_length > 1 && !is_boot_path(path, path_length)) {
            mode &= 0777U;
            if (header->typeflag == '5') {
                if (create_parent_directories(path) != 0 ||
                    okfs_fileops_mkdir(path, mode == 0 ? 0755 : mode) != 0) {
                    return -1;
                }
            } else if (header->typeflag == '\0' ||
                       header->typeflag == '0') {
                if (create_parent_directories(path) != 0 ||
                    okfs_fileops_write(
                        path,
                        (const void *)(uintptr_t)(tar_address + 512),
                        size) < 0 ||
                    okfs_fileops_chmod(path, mode == 0 ? 0644 : mode) != 0) {
                    return -1;
                }
                unpacked++;
            }
        }

        tar_address += 512U + ((size + 511U) / 512U) * 512U;
        header = (struct tar_header *)(uintptr_t)tar_address;
    }

    return (int)unpacked;
}
