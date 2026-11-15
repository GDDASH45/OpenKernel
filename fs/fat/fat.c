#include <fs/fat/fat.h>
#include <kernel/ata.h>
#include <stdint.h>

#define FAT_SECTOR_SIZE       512u
#define FAT_DIRECTORY_SIZE    32u
#define FAT_NAME_LENGTH       11u
#define FAT_MAX_COMPONENT     12u
#define FAT_ATTR_DIRECTORY    0x10u
#define FAT_ATTR_VOLUME       0x08u
#define FAT_ATTR_LONG_NAME    0x0Fu
#define FAT_CLUSTER_EOC       0xFFF8u
#define FAT_CLUSTER_BAD       0xFFF7u
#define FAT_CLUSTER_RESERVED  0xFFF0u
#define FAT_MAX_PATH          256u

struct fat16_volume {
    uint32_t partition_lba;
    uint32_t total_sectors;
    uint32_t fat_start;
    uint32_t root_start;
    uint32_t data_start;
    uint32_t fat_sectors;
    uint32_t root_entries;
    uint32_t root_sectors;
    uint32_t cluster_count;
    uint16_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint8_t mounted;
};

struct fat_directory_entry {
    uint8_t name[11];
    uint8_t attributes;
    uint8_t reserved[8];
    uint16_t first_cluster;
    uint32_t file_size;
} __attribute__((packed));

typedef char fat_directory_entry_must_be_32_bytes[
    sizeof(struct fat_directory_entry) == FAT_DIRECTORY_SIZE ? 1 : -1];

static struct fat16_volume fat_volume;
static uint8_t fat_sector[FAT_SECTOR_SIZE];

static uint16_t fat_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t fat_u32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static int fat_read_volume_sector(uint32_t sector, void *buffer)
{
    if (!fat_volume.mounted || sector >= fat_volume.total_sectors ||
        fat_volume.partition_lba > 0xFFFFFFFFu - sector) {
        return -1;
    }
    return ata_read_sector(fat_volume.partition_lba + sector, buffer);
}

static int fat_valid_cluster(uint16_t cluster)
{
    return cluster >= 2u && cluster < fat_volume.cluster_count + 2u &&
           cluster < FAT_CLUSTER_RESERVED;
}

static int fat_next_cluster(uint16_t cluster, uint16_t *next)
{
    uint32_t byte_offset = (uint32_t)cluster * 2u;
    uint32_t sector = fat_volume.fat_start + byte_offset / FAT_SECTOR_SIZE;
    uint32_t offset = byte_offset % FAT_SECTOR_SIZE;

    if (next == 0 || byte_offset / FAT_SECTOR_SIZE >= fat_volume.fat_sectors ||
        fat_read_volume_sector(sector, fat_sector) != 0) {
        return -1;
    }

    *next = fat_u16(&fat_sector[offset]);
    if (*next >= FAT_CLUSTER_EOC) {
        return 1;
    }
    if (*next == FAT_CLUSTER_BAD || *next < 2u ||
        *next >= FAT_CLUSTER_RESERVED || !fat_valid_cluster(*next)) {
        return -1;
    }
    return 0;
}

static int fat_read_cluster_sector(uint16_t cluster, uint32_t sector_index,
                                   void *buffer)
{
    uint32_t first_sector;

    if (!fat_valid_cluster(cluster) ||
        sector_index >= fat_volume.sectors_per_cluster) {
        return -1;
    }

    first_sector = fat_volume.data_start +
                   ((uint32_t)cluster - 2u) *
                       fat_volume.sectors_per_cluster;
    return fat_read_volume_sector(first_sector + sector_index, buffer);
}

static int fat_make_short_name(const char *component, uint8_t name[11])
{
    uint32_t input = 0;
    uint32_t output = 0;
    int extension = 0;

    if (component == 0 || component[0] == '\0') {
        return -1;
    }

    for (uint32_t index = 0; index < FAT_NAME_LENGTH; index++) {
        name[index] = ' ';
    }

    while (component[input] != '\0') {
        char character = component[input++];

        if (character == '.') {
            if (extension || output == 0 || output > 8) {
                return -1;
            }
            extension = 1;
            output = 8;
            continue;
        }

        if (character == '/' || character == '\\' ||
            character == ' ' || character < 0x21 || character > 0x7E ||
            character == '*' || character == '?' || character == ':' ||
            character == '"' || character == '<' || character == '>' ||
            character == '|') {
            return -1;
        }

        if (character >= 'a' && character <= 'z') {
            character = (char)(character - 'a' + 'A');
        }

        if ((!extension && output >= 8) || (extension && output >= 11)) {
            return -1;
        }
        name[output++] = (uint8_t)character;
    }

    return 0;
}

static int fat_entry_matches(const struct fat_directory_entry *entry,
                             const uint8_t name[11])
{
    for (uint32_t index = 0; index < FAT_NAME_LENGTH; index++) {
        if (entry->name[index] != name[index]) {
            return 0;
        }
    }
    return 1;
}

static int fat_find_in_sector(uint32_t sector, const uint8_t name[11],
                              struct fat_directory_entry *result,
                              int *finished)
{
    if (fat_read_volume_sector(sector, fat_sector) != 0) {
        return -1;
    }

    for (uint32_t offset = 0; offset < FAT_SECTOR_SIZE;
         offset += FAT_DIRECTORY_SIZE) {
        struct fat_directory_entry *entry =
            (struct fat_directory_entry *)&fat_sector[offset];

        if (entry->name[0] == 0x00) {
            *finished = 1;
            return 0;
        }
        if (entry->name[0] == 0xE5 ||
            entry->attributes == FAT_ATTR_LONG_NAME ||
            (entry->attributes & FAT_ATTR_VOLUME)) {
            continue;
        }
        if (fat_entry_matches(entry, name)) {
            *result = *entry;
            return 1;
        }
    }
    return 0;
}

static int fat_find_entry(uint16_t directory_cluster, int root_directory,
                          const uint8_t name[11],
                          struct fat_directory_entry *result)
{
    int finished = 0;

    if (root_directory) {
        uint32_t entries_seen = 0;

        for (uint32_t sector = 0; sector < fat_volume.root_sectors; sector++) {
            uint32_t absolute = fat_volume.root_start + sector;
            int found = fat_find_in_sector(absolute, name, result, &finished);

            if (found != 0 || finished) {
                return found;
            }
            entries_seen += FAT_SECTOR_SIZE / FAT_DIRECTORY_SIZE;
            if (entries_seen >= fat_volume.root_entries) {
                break;
            }
        }
        return 0;
    }

    if (!fat_valid_cluster(directory_cluster)) {
        return -1;
    }

    for (uint32_t chain_count = 0;
         chain_count < fat_volume.cluster_count; chain_count++) {
        for (uint32_t sector = 0;
             sector < fat_volume.sectors_per_cluster; sector++) {
            uint32_t first_sector =
                fat_volume.data_start +
                ((uint32_t)directory_cluster - 2u) *
                    fat_volume.sectors_per_cluster;
            int found = fat_find_in_sector(first_sector + sector, name,
                                           result, &finished);

            if (found != 0 || finished) {
                return found;
            }
        }

        uint16_t next;
        int status = fat_next_cluster(directory_cluster, &next);
        if (status == 1) {
            return 0;
        }
        if (status != 0) {
            return -1;
        }
        directory_cluster = next;
    }

    return -1;
}

static int fat_resolve(const char *path, struct fat_directory_entry *entry,
                       int *is_root)
{
    uint16_t directory_cluster = 0;
    int root_directory = 1;
    uint32_t position = 0;
    char component[FAT_MAX_COMPONENT + 1];

    if (path == 0 || path[0] != '/') {
        return -1;
    }

    while (path[position] == '/') {
        position++;
    }
    if (path[position] == '\0') {
        if (is_root != 0) {
            *is_root = 1;
        }
        return 0;
    }

    for (;;) {
        uint32_t length = 0;
        uint8_t short_name[11];
        struct fat_directory_entry found;

        while (path[position] != '\0' && path[position] != '/') {
            if (length >= FAT_MAX_COMPONENT) {
                return -1;
            }
            component[length++] = path[position++];
        }
        component[length] = '\0';

        if (fat_make_short_name(component, short_name) != 0 ||
            fat_find_entry(directory_cluster, root_directory,
                           short_name, &found) != 1) {
            return -1;
        }

        while (path[position] == '/') {
            position++;
        }
        if (path[position] == '\0') {
            if (entry != 0) {
                *entry = found;
            }
            if (is_root != 0) {
                *is_root = 0;
            }
            return 0;
        }

        if (!(found.attributes & FAT_ATTR_DIRECTORY) ||
            !fat_valid_cluster(found.first_cluster)) {
            return -1;
        }
        directory_cluster = found.first_cluster;
        root_directory = 0;
    }
}

int fat_mount(uint32_t partition_lba)
{
    uint8_t boot_sector[FAT_SECTOR_SIZE];
    uint16_t bytes_per_sector;
    uint16_t sectors_per_fat;
    uint32_t total_sectors;
    uint32_t root_sectors;
    uint32_t data_sectors;
    uint32_t cluster_count;

    fat_volume.mounted = 0;

    if (ata_read_sector(partition_lba, boot_sector) != 0 ||
        boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        return -1;
    }

    bytes_per_sector = fat_u16(&boot_sector[11]);
    fat_volume.sectors_per_cluster = boot_sector[13];
    fat_volume.reserved_sectors = fat_u16(&boot_sector[14]);
    fat_volume.fat_count = boot_sector[16];
    fat_volume.root_entries = fat_u16(&boot_sector[17]);
    total_sectors = fat_u16(&boot_sector[19]);
    if (total_sectors == 0) {
        total_sectors = fat_u32(&boot_sector[32]);
    }
    sectors_per_fat = fat_u16(&boot_sector[22]);

    if (bytes_per_sector != FAT_SECTOR_SIZE ||
        fat_volume.sectors_per_cluster == 0 ||
        (fat_volume.sectors_per_cluster &
         (fat_volume.sectors_per_cluster - 1u)) != 0 ||
        fat_volume.sectors_per_cluster > 128u ||
        fat_volume.reserved_sectors == 0 ||
        fat_volume.fat_count == 0 || fat_volume.fat_count > 2 ||
        fat_volume.root_entries == 0 || sectors_per_fat == 0 ||
        total_sectors == 0) {
        return -1;
    }

    root_sectors =
        ((uint32_t)fat_volume.root_entries * FAT_DIRECTORY_SIZE +
         (FAT_SECTOR_SIZE - 1u)) / FAT_SECTOR_SIZE;

    if (fat_volume.reserved_sectors >
            0xFFFFFFFFu - (uint32_t)fat_volume.fat_count * sectors_per_fat ||
        (uint32_t)fat_volume.reserved_sectors +
                (uint32_t)fat_volume.fat_count * sectors_per_fat >
            0xFFFFFFFFu - root_sectors) {
        return -1;
    }

    data_sectors = total_sectors -
        ((uint32_t)fat_volume.reserved_sectors +
         (uint32_t)fat_volume.fat_count * sectors_per_fat +
         root_sectors);
    if (total_sectors <=
        (uint32_t)fat_volume.reserved_sectors +
        (uint32_t)fat_volume.fat_count * sectors_per_fat + root_sectors) {
        return -1;
    }

    cluster_count = data_sectors / fat_volume.sectors_per_cluster;

    /* FAT16 volumes contain 4,085 through 65,524 data clusters. */
    if (cluster_count < 4085u || cluster_count >= 65525u ||
        partition_lba > 0xFFFFFFFFu - total_sectors) {
        return -1;
    }

    fat_volume.partition_lba = partition_lba;
    fat_volume.total_sectors = total_sectors;
    fat_volume.fat_start = fat_volume.reserved_sectors;
    fat_volume.fat_sectors = sectors_per_fat;
    fat_volume.root_start = fat_volume.fat_start +
                            (uint32_t)fat_volume.fat_count * sectors_per_fat;
    fat_volume.root_sectors = root_sectors;
    fat_volume.data_start = fat_volume.root_start + root_sectors;
    fat_volume.cluster_count = cluster_count;

    if ((cluster_count + 2u) * 2u >
        fat_volume.fat_sectors * FAT_SECTOR_SIZE) {
        return -1;
    }

    fat_volume.mounted = 1;
    return 0;
}

int fat_is_mounted(void)
{
    return fat_volume.mounted != 0;
}

int fat_read_file(const char *path, void *buffer, uint32_t capacity)
{
    struct fat_directory_entry entry;
    uint32_t copied = 0;
    uint16_t cluster;
    int is_root;

    if (!fat_volume.mounted || buffer == 0 ||
        fat_resolve(path, &entry, &is_root) != 0 || is_root ||
        (entry.attributes & FAT_ATTR_DIRECTORY)) {
        return -1;
    }
    if (entry.file_size > capacity) {
        return -22;
    }
    if (entry.file_size == 0) {
        return 0;
    }
    if (!fat_valid_cluster(entry.first_cluster)) {
        return -1;
    }

    cluster = entry.first_cluster;
    while (copied < entry.file_size) {
        for (uint32_t sector = 0;
             sector < fat_volume.sectors_per_cluster &&
             copied < entry.file_size; sector++) {
            uint32_t amount = entry.file_size - copied;

            if (amount > FAT_SECTOR_SIZE) {
                amount = FAT_SECTOR_SIZE;
            }
            if (fat_read_cluster_sector(cluster, sector, fat_sector) != 0) {
                return -1;
            }
            for (uint32_t byte = 0; byte < amount; byte++) {
                ((uint8_t *)buffer)[copied + byte] = fat_sector[byte];
            }
            copied += amount;
        }

        if (copied < entry.file_size) {
            uint16_t next;
            if (fat_next_cluster(cluster, &next) != 0) {
                return -1;
            }
            cluster = next;
        }
    }

    return (int)copied;
}

static uint32_t fat_format_name(const struct fat_directory_entry *entry,
                                char *buffer, uint32_t capacity)
{
    uint32_t used = 0;
    uint32_t base_length = 8;
    uint32_t extension_length = 3;

    while (base_length != 0 && entry->name[base_length - 1] == ' ') {
        base_length--;
    }
    while (extension_length != 0 &&
           entry->name[8 + extension_length - 1] == ' ') {
        extension_length--;
    }

    for (uint32_t index = 0; index < base_length && used < capacity; index++) {
        buffer[used++] = (char)entry->name[index];
    }
    if (extension_length != 0 && used < capacity) {
        buffer[used++] = '.';
        for (uint32_t index = 0;
             index < extension_length && used < capacity; index++) {
            buffer[used++] = (char)entry->name[8 + index];
        }
    }
    if ((entry->attributes & FAT_ATTR_DIRECTORY) && used < capacity) {
        buffer[used++] = '/';
    }

    return used;
}

static int fat_list_sector(uint32_t sector, char *buffer,
                           uint32_t capacity, uint32_t *used,
                           int *finished)
{
    if (fat_read_volume_sector(sector, fat_sector) != 0) {
        return -1;
    }

    for (uint32_t offset = 0; offset < FAT_SECTOR_SIZE;
         offset += FAT_DIRECTORY_SIZE) {
        struct fat_directory_entry *entry =
            (struct fat_directory_entry *)&fat_sector[offset];
        uint32_t amount;

        if (entry->name[0] == 0x00) {
            *finished = 1;
            return 0;
        }
        if (entry->name[0] == 0xE5 ||
            entry->attributes == FAT_ATTR_LONG_NAME ||
            (entry->attributes & FAT_ATTR_VOLUME)) {
            continue;
        }

        amount = fat_format_name(entry, buffer + *used,
                                 capacity - *used);
        if (amount == 0 || amount + 1u > capacity - *used) {
            return -22;
        }
        *used += amount;
        buffer[(*used)++] = '\n';
    }
    return 0;
}

int fat_list_directory(const char *path, char *buffer, uint32_t capacity)
{
    struct fat_directory_entry entry;
    uint16_t cluster = 0;
    uint32_t used = 0;
    int root_directory;
    int finished = 0;

    if (!fat_volume.mounted || buffer == 0 || capacity == 0 ||
        fat_resolve(path, &entry, &root_directory) != 0) {
        return -1;
    }

    if (!root_directory) {
        if (!(entry.attributes & FAT_ATTR_DIRECTORY) ||
            !fat_valid_cluster(entry.first_cluster)) {
            return -1;
        }
        cluster = entry.first_cluster;
    }

    if (root_directory) {
        uint32_t entries_seen = 0;

        for (uint32_t sector = 0; sector < fat_volume.root_sectors; sector++) {
            int status = fat_list_sector(fat_volume.root_start + sector,
                                         buffer, capacity, &used, &finished);
            if (status != 0 || finished) {
                return status != 0 ? status : (int)used;
            }
            entries_seen += FAT_SECTOR_SIZE / FAT_DIRECTORY_SIZE;
            if (entries_seen >= fat_volume.root_entries) {
                break;
            }
        }
        return (int)used;
    }

    for (uint32_t chain_count = 0;
         chain_count < fat_volume.cluster_count; chain_count++) {
        for (uint32_t sector = 0;
             sector < fat_volume.sectors_per_cluster; sector++) {
            uint32_t first_sector =
                fat_volume.data_start +
                ((uint32_t)cluster - 2u) *
                    fat_volume.sectors_per_cluster;
            int status = fat_list_sector(first_sector + sector, buffer,
                                         capacity, &used, &finished);
            if (status != 0 || finished) {
                return status != 0 ? status : (int)used;
            }
        }

        uint16_t next;
        int status = fat_next_cluster(cluster, &next);
        if (status == 1) {
            return (int)used;
        }
        if (status != 0) {
            return -1;
        }
        cluster = next;
    }

    return -1;
}
