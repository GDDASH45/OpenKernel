#include <kernel/ata.h>
#include <kernel/okfs.h>
#include <kernel/tar.h>

#define OKFS_MAGIC "OKFS001"
#define OKFS_MAGIC_SIZE 8
#define OKFS_SUPER_LBA 2048
#define OKFS_DIRECTORY_LBA 2049
#define OKFS_DIRECTORY_SECTORS 4
#define OKFS_DATA_LBA (OKFS_DIRECTORY_LBA + OKFS_DIRECTORY_SECTORS)
#define OKFS_MAX_FILES 64
#define OKFS_NAME_SIZE 48
#define OKFS_FLAG_USED 1

struct okfs_superblock {
    char magic[OKFS_MAGIC_SIZE];
    uint32_t version;
    uint32_t directory_lba;
    uint32_t data_lba;
    uint32_t max_files;
    uint32_t reserved[123];
} __attribute__((packed));

struct okfs_entry {
    char name[OKFS_NAME_SIZE];
    uint32_t size;
    uint32_t start_lba;
    uint32_t sectors;
    uint32_t flags;
} __attribute__((packed));

static int mounted;

static int text_equal(const char *left, const char *right) {
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static int valid_path(const char *path) {
    return path != 0 && path[0] == '/' && path[1] != '\0' &&
           path[1] != '/' && path[1] != '.';
}

static int load_directory(struct okfs_entry *entries) {
    uint16_t sector[256];
    uint8_t *destination = (uint8_t *)entries;

    for (uint32_t index = 0; index < OKFS_DIRECTORY_SECTORS; index++) {
        if (ata_read_sector(OKFS_DIRECTORY_LBA + index, sector) != 0) {
            return -1;
        }
        for (uint32_t byte = 0; byte < 512; byte++) {
            destination[index * 512 + byte] = ((uint8_t *)sector)[byte];
        }
    }
    return 0;
}

static int save_directory(const struct okfs_entry *entries) {
    const uint8_t *source = (const uint8_t *)entries;
    uint16_t sector[256];

    for (uint32_t index = 0; index < OKFS_DIRECTORY_SECTORS; index++) {
        for (uint32_t byte = 0; byte < 512; byte++) {
            ((uint8_t *)sector)[byte] = source[index * 512 + byte];
        }
        if (ata_write_sector(OKFS_DIRECTORY_LBA + index, sector) != 0) {
            return -1;
        }
    }
    return 0;
}

static int find_entry(const struct okfs_entry *entries, const char *path) {
    for (int index = 0; index < OKFS_MAX_FILES; index++) {
        if ((entries[index].flags & OKFS_FLAG_USED) != 0 &&
            text_equal(entries[index].name, path + 1)) {
            return index;
        }
    }
    return -1;
}

static uint32_t next_data_lba(const struct okfs_entry *entries) {
    uint32_t next = OKFS_DATA_LBA;

    for (int index = 0; index < OKFS_MAX_FILES; index++) {
        if ((entries[index].flags & OKFS_FLAG_USED) != 0 &&
            entries[index].start_lba + entries[index].sectors > next) {
            next = entries[index].start_lba + entries[index].sectors;
        }
    }
    return next;
}

static int format_filesystem(void) {
    struct okfs_superblock superblock;
    struct okfs_entry entries[OKFS_MAX_FILES];

    for (uint32_t byte = 0; byte < sizeof(superblock); byte++) {
        ((uint8_t *)&superblock)[byte] = 0;
    }
    for (uint32_t byte = 0; byte < OKFS_MAGIC_SIZE; byte++) {
        superblock.magic[byte] = OKFS_MAGIC[byte];
    }
    superblock.version = 1;
    superblock.directory_lba = OKFS_DIRECTORY_LBA;
    superblock.data_lba = OKFS_DATA_LBA;
    superblock.max_files = OKFS_MAX_FILES;

    for (uint32_t byte = 0; byte < sizeof(entries); byte++) {
        ((uint8_t *)entries)[byte] = 0;
    }
    if (ata_write_sector(OKFS_SUPER_LBA, &superblock) != 0) {
        return -1;
    }
    return save_directory(entries);
}

int okfs_mount(void) {
    struct okfs_superblock superblock;

    if (ata_read_sector(OKFS_SUPER_LBA, &superblock) != 0 ||
        !text_equal(superblock.magic, OKFS_MAGIC)) {
        if (format_filesystem() != 0) {
            return -1;
        }
    }
    mounted = 1;
    return 0;
}

int okfs_read(const char *path, void *buffer, uint32_t capacity) {
    struct okfs_entry entries[OKFS_MAX_FILES];
    uint16_t sector[256];
    uint8_t *destination = (uint8_t *)buffer;
    int entry_index;
    uint32_t copied = 0;

    if (!mounted || !valid_path(path) || buffer == 0 ||
        load_directory(entries) != 0) {
        return -1;
    }
    entry_index = find_entry(entries, path);
    if (entry_index < 0) {
        return -1;
    }
    if (capacity < entries[entry_index].size) {
        return -2;
    }

    while (copied < entries[entry_index].size) {
        uint32_t amount = entries[entry_index].size - copied;
        if (amount > 512) amount = 512;
        if (ata_read_sector(entries[entry_index].start_lba + copied / 512, sector) != 0) {
            return -1;
        }
        for (uint32_t byte = 0; byte < amount; byte++) {
            destination[copied + byte] = ((uint8_t *)sector)[byte];
        }
        copied += amount;
    }
    return (int)copied;
}

int okfs_write(const char *path, const void *data, uint32_t size) {
    struct okfs_entry entries[OKFS_MAX_FILES];
    uint16_t sector[256];
    const uint8_t *source = (const uint8_t *)data;
    int entry_index;
    uint32_t sectors;

    if (!mounted || !valid_path(path) || data == 0 || size > 512 * 128 ||
        load_directory(entries) != 0) {
        return -1;
    }
    entry_index = find_entry(entries, path);
    if (entry_index < 0) {
        for (entry_index = 0; entry_index < OKFS_MAX_FILES; entry_index++) {
            if ((entries[entry_index].flags & OKFS_FLAG_USED) == 0) break;
        }
        if (entry_index == OKFS_MAX_FILES) return -1;
        for (uint32_t byte = 0; byte < OKFS_NAME_SIZE; byte++) {
            entries[entry_index].name[byte] = 0;
        }
        for (uint32_t byte = 0; byte < OKFS_NAME_SIZE - 1 && path[byte + 1] != '\0'; byte++) {
            entries[entry_index].name[byte] = path[byte + 1];
        }
        entries[entry_index].start_lba = next_data_lba(entries);
        entries[entry_index].sectors = (size + 511) / 512;
        entries[entry_index].flags = OKFS_FLAG_USED;
    } else if (size > entries[entry_index].sectors * 512) {
        entries[entry_index].start_lba = next_data_lba(entries);
        entries[entry_index].sectors = (size + 511) / 512;
    }

    sectors = (size + 511) / 512;
    for (uint32_t index = 0; index < sectors; index++) {
        for (uint32_t byte = 0; byte < 512; byte++) {
            uint32_t source_index = index * 512 + byte;
            ((uint8_t *)sector)[byte] = source_index < size ? source[source_index] : 0;
        }
        if (ata_write_sector(entries[entry_index].start_lba + index, sector) != 0) {
            return -1;
        }
    }
    entries[entry_index].size = size;
    return save_directory(entries) == 0 ? (int)size : -1;
}

static uint32_t tar_size(const char *field) {
    uint32_t size = 0;

    for (int index = 0; index < 11; index++) {
        if (field[index] >= '0' && field[index] <= '7') {
            size = size * 8 + (uint32_t)(field[index] - '0');
        }
    }
    return size;
}

int okfs_unpack_tar(uint32_t tar_address) {
    struct tar_header *header = (struct tar_header *)(uintptr_t)tar_address;
    uint32_t unpacked = 0;

    while (header->name[0] != '\0') {
        uint32_t size = tar_size(header->size);
        const char *name = header->name;
        char path[OKFS_NAME_SIZE];
        uint32_t name_length = 0;

        if (name[0] == '.' && name[1] == '/') {
            name += 2;
        }
        while (name[name_length] != '\0' && name_length < OKFS_NAME_SIZE - 2) {
            name_length++;
        }

        if (name_length > 0 && name[0] != '\0' &&
            !(name[0] == 'b' && name[1] == 'o' && name[2] == 'o' &&
              name[3] == 't' && (name[4] == '/' || name[4] == '\0')) &&
            (header->typeflag == '\0' || header->typeflag == '0') &&
            name[name_length - 1] != '/') {
            path[0] = '/';
            for (uint32_t index = 0; index < name_length; index++) {
                path[index + 1] = name[index];
            }
            path[name_length + 1] = '\0';
            if (okfs_write(path, (const void *)(uintptr_t)(tar_address + 512), size) < 0) {
                return -1;
            }
            unpacked++;
        }

        tar_address += 512 + ((size + 511) / 512) * 512;
        header = (struct tar_header *)(uintptr_t)tar_address;
    }
    return (int)unpacked;
}