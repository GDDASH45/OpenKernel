#include <fs/fat/fat.h>
#include <kernel/ata.h>
#include <stdint.h>

#define FAT_SECTOR_SIZE 512u
#define FAT_ENTRY_SIZE 32u
#define FAT_MAX_PATH 256u
#define FAT_ATTR_DIRECTORY 0x10u
#define FAT_ATTR_VOLUME 0x08u
#define FAT_ATTR_LFN 0x0Fu

enum { FAT12 = 12, FAT16 = 16, FAT32 = 32 };

struct fat_volume {
    uint32_t partition_lba, total_sectors, fat_start, fat_sectors;
    uint32_t root_start, root_sectors, root_cluster, data_start;
    uint32_t cluster_count;
    uint16_t sectors_per_cluster, root_entries, reserved_sectors;
    uint8_t fat_count, type, mounted;
};

struct fat_entry {
    uint8_t name[11];
    uint8_t attributes;
    uint8_t nt_reserved;
    uint8_t creation[7];
    uint16_t cluster_high;
    uint8_t modified[4];
    uint16_t cluster_low;
    uint32_t size;
} __attribute__((packed));

static struct fat_volume volume;
static uint8_t sector_buffer[FAT_SECTOR_SIZE];

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int volume_read(uint32_t lba, void *buffer)
{
    if (!volume.mounted || lba >= volume.total_sectors ||
        volume.partition_lba > 0xFFFFFFFFu - lba)
        return -1;
    return ata_read_sector(volume.partition_lba + lba, buffer);
}

static int volume_write(uint32_t lba, const void *buffer)
{
    if (!volume.mounted || lba >= volume.total_sectors ||
        volume.partition_lba > 0xFFFFFFFFu - lba)
        return -1;
    return ata_write_sector(volume.partition_lba + lba, buffer);
}

static int valid_cluster(uint32_t cluster)
{
    return cluster >= 2 && cluster < volume.cluster_count + 2;
}

static int fat_get(uint32_t cluster, uint32_t *value)
{
    uint32_t offset, lba, within;
    uint8_t pair[2];
    uint16_t word;

    if (!valid_cluster(cluster) || value == 0)
        return -1;

    if (volume.type == FAT12) {
        offset = cluster + cluster / 2;
        lba = volume.fat_start + offset / FAT_SECTOR_SIZE;
        within = offset % FAT_SECTOR_SIZE;
        if (volume_read(lba, sector_buffer) != 0)
            return -1;
        pair[0] = sector_buffer[within];
        if (within == FAT_SECTOR_SIZE - 1) {
            if (volume_read(lba + 1, sector_buffer) != 0)
                return -1;
            pair[1] = sector_buffer[0];
        } else {
            pair[1] = sector_buffer[within + 1];
        }
        word = get16(pair);
        *value = (cluster & 1) ? (word >> 4) : (word & 0x0FFF);
        return 0;
    }

    offset = cluster * (volume.type == FAT16 ? 2u : 4u);
    if (offset / FAT_SECTOR_SIZE >= volume.fat_sectors)
        return -1;
    if (volume_read(volume.fat_start + offset / FAT_SECTOR_SIZE,
                    sector_buffer) != 0)
        return -1;
    within = offset % FAT_SECTOR_SIZE;
    *value = volume.type == FAT16 ? get16(&sector_buffer[within]) :
             (get32(&sector_buffer[within]) & 0x0FFFFFFF);
    return 0;
}

static int fat_eoc(uint32_t value)
{
    if (volume.type == FAT12) return value >= 0x0FF8;
    if (volume.type == FAT16) return value >= 0xFFF8;
    return value >= 0x0FFFFFF8;
}

static int fat_next(uint32_t cluster, uint32_t *next)
{
    uint32_t value;
    if (fat_get(cluster, &value) != 0)
        return -1;
    if (fat_eoc(value))
        return 1;
    if (!valid_cluster(value))
        return -1;
    *next = value;
    return 0;
}

static uint32_t entry_cluster(const struct fat_entry *entry)
{
    return ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
}

static int make_name(const char *component, uint8_t name[11])
{
    uint32_t i = 0, out = 0;
    int extension = 0;

    if (!component || !component[0])
        return -1;
    for (i = 0; i < 11; ++i) name[i] = ' ';
    i = 0;
    while (component[i]) {
        char c = component[i++];
        if (c == '.') {
            if (extension || out == 0 || out > 8) return -1;
            extension = 1;
            out = 8;
            continue;
        }
        if (c <= 0x20 || c > 0x7E || c == '/' || c == '\\' ||
            c == '*' || c == '?' || c == ':' || c == '"' ||
            c == '<' || c == '>' || c == '|')
            return -1;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if ((!extension && out >= 8) || (extension && out >= 11))
            return -1;
        name[out++] = (uint8_t)c;
    }
    return 0;
}

static int find_in_sector(uint32_t lba, const uint8_t name[11],
                          struct fat_entry *found, int *end)
{
    if (volume_read(lba, sector_buffer) != 0) return -1;
    for (uint32_t off = 0; off < FAT_SECTOR_SIZE; off += FAT_ENTRY_SIZE) {
        struct fat_entry *e = (struct fat_entry *)&sector_buffer[off];
        if (e->name[0] == 0) { *end = 1; return 0; }
        if (e->name[0] == 0xE5 || e->attributes == FAT_ATTR_LFN ||
            (e->attributes & FAT_ATTR_VOLUME))
            continue;
        uint32_t i;
        for (i = 0; i < 11 && e->name[i] == name[i]; ++i) {}
        if (i == 11) { *found = *e; return 1; }
    }
    return 0;
}

static int find_entry(uint32_t directory, int fixed_root,
                      const uint8_t name[11], struct fat_entry *found)
{
    int end = 0;
    if (fixed_root) {
        for (uint32_t i = 0; i < volume.root_sectors; ++i) {
            int r = find_in_sector(volume.root_start + i, name, found, &end);
            if (r || end) return r;
        }
        return 0;
    }
    for (uint32_t count = 0; count < volume.cluster_count; ++count) {
        uint32_t first = volume.data_start +
            (directory - 2) * volume.sectors_per_cluster;
        for (uint32_t s = 0; s < volume.sectors_per_cluster; ++s) {
            int r = find_in_sector(first + s, name, found, &end);
            if (r || end) return r;
        }
        uint32_t next;
        int r = fat_next(directory, &next);
        if (r == 1) return 0;
        if (r != 0) return -1;
        directory = next;
    }
    return -1;
}

static int resolve(const char *path, struct fat_entry *entry, int *root)
{
    uint32_t pos = 0, directory = volume.root_cluster;
    int fixed_root = volume.type != FAT32;
    char component[13];

    if (!path || path[0] != '/') return -1;
    while (path[pos] == '/') ++pos;
    if (!path[pos]) { if (root) *root = 1; return 0; }

    for (;;) {
        uint32_t n = 0;
        uint8_t name[11];
        struct fat_entry e;
        while (path[pos] && path[pos] != '/') {
            if (n >= 12) return -1;
            component[n++] = path[pos++];
        }
        component[n] = 0;
        if (make_name(component, name) != 0 ||
            find_entry(directory, fixed_root, name, &e) != 1)
            return -1;
        while (path[pos] == '/') ++pos;
        if (!path[pos]) {
            if (entry) *entry = e;
            if (root) *root = 0;
            return 0;
        }
        if (!(e.attributes & FAT_ATTR_DIRECTORY) ||
            !valid_cluster(entry_cluster(&e)))
            return -1;
        directory = entry_cluster(&e);
        fixed_root = 0;
    }
}

static int data_sector(uint32_t cluster, uint32_t index, uint32_t *lba)
{
    if (!valid_cluster(cluster) || index >= volume.sectors_per_cluster)
        return -1;
    *lba = volume.data_start + (cluster - 2) *
           volume.sectors_per_cluster + index;
    return 0;
}

int fat_mount(uint32_t partition_lba)
{
    uint8_t b[512];
    uint32_t total, fat_size, root_sectors, data_sectors, clusters;
    uint16_t bytes, root_entries;

    volume.mounted = 0;
    if (ata_read_sector(partition_lba, b) != 0 ||
        b[510] != 0x55 || b[511] != 0xAA)
        return -1;

    bytes = get16(&b[11]);
    volume.sectors_per_cluster = b[13];
    volume.reserved_sectors = get16(&b[14]);
    volume.fat_count = b[16];
    root_entries = get16(&b[17]);
    total = get16(&b[19]);
    if (!total) total = get32(&b[32]);
    fat_size = get16(&b[22]);
    if (!fat_size) fat_size = get32(&b[36]);

    if (bytes != 512 || !volume.sectors_per_cluster ||
        (volume.sectors_per_cluster & (volume.sectors_per_cluster - 1)) ||
        volume.sectors_per_cluster > 128 || !volume.reserved_sectors ||
        !volume.fat_count || volume.fat_count > 2 || !fat_size || !total)
        return -1;

    root_sectors = ((uint32_t)root_entries * 32 + 511) / 512;
    if ((uint32_t)volume.reserved_sectors +
        (uint32_t)volume.fat_count * fat_size + root_sectors >= total)
        return -1;
    data_sectors = total - volume.reserved_sectors -
                   (uint32_t)volume.fat_count * fat_size - root_sectors;
    clusters = data_sectors / volume.sectors_per_cluster;
    if (clusters < 1 || clusters >= 0x0FFFFFF5u ||
        partition_lba > 0xFFFFFFFFu - total)
        return -1;

    volume.type = clusters < 4085 ? FAT12 :
                  (clusters < 65525 ? FAT16 : FAT32);
    if ((volume.type == FAT32 && root_entries != 0) ||
        (volume.type != FAT32 && root_entries == 0))
        return -1;

    volume.partition_lba = partition_lba;
    volume.total_sectors = total;
    volume.fat_start = volume.reserved_sectors;
    volume.fat_sectors = fat_size;
    volume.root_entries = root_entries;
    volume.root_sectors = root_sectors;
    volume.root_start = volume.fat_start + volume.fat_count * fat_size;
    volume.data_start = volume.root_start + root_sectors;
    volume.cluster_count = clusters;
    volume.root_cluster = volume.type == FAT32 ?
                          (get32(&b[44]) & 0x0FFFFFFF) : 0;

    if (volume.type == FAT32 && !valid_cluster(volume.root_cluster))
        return -1;
    if (volume.type == FAT12 &&
        ((clusters + 2) * 3 / 2 + 1) > fat_size * 512)
        return -1;
    if (volume.type == FAT16 && (clusters + 2) * 2 > fat_size * 512)
        return -1;
    if (volume.type == FAT32 && (clusters + 2) * 4 > fat_size * 512)
        return -1;

    volume.mounted = 1;
    return 0;
}

int fat_is_mounted(void) { return volume.mounted != 0; }

int fat_read_file(const char *path, void *buffer, uint32_t capacity)
{
    struct fat_entry e;
    uint32_t cluster, copied = 0;
    int root;
    if (!volume.mounted || !buffer || resolve(path, &e, &root) ||
        root || (e.attributes & FAT_ATTR_DIRECTORY))
        return -1;
    if (e.size > 0x7FFFFFFFu) return -1;
    if (e.size > capacity) return -22;
    if (!e.size) return 0;
    cluster = entry_cluster(&e);
    if (!valid_cluster(cluster)) return -1;

    while (copied < e.size) {
        for (uint32_t s = 0; s < volume.sectors_per_cluster &&
             copied < e.size; ++s) {
            uint32_t lba, amount = e.size - copied;
            if (amount > 512) amount = 512;
            if (data_sector(cluster, s, &lba) ||
                volume_read(lba, sector_buffer)) return -1;
            for (uint32_t i = 0; i < amount; ++i)
                ((uint8_t *)buffer)[copied + i] = sector_buffer[i];
            copied += amount;
        }
        if (copied < e.size) {
            uint32_t next;
            if (fat_next(cluster, &next)) return -1;
            cluster = next;
        }
    }
    return (int)copied;
}

/* Overwrite an existing file without changing its allocated size. */
int fat_write_file(const char *path, const void *buffer, uint32_t size)
{
    struct fat_entry e;
    uint32_t cluster, copied = 0;
    int root;
    if (!volume.mounted || (!buffer && size) ||
        resolve(path, &e, &root) || root ||
        (e.attributes & FAT_ATTR_DIRECTORY) || size != e.size ||
        size > 0x7FFFFFFFu)
        return -1;
    if (!size) return 0;
    cluster = entry_cluster(&e);
    if (!valid_cluster(cluster)) return -1;

    while (copied < size) {
        for (uint32_t s = 0; s < volume.sectors_per_cluster &&
             copied < size; ++s) {
            uint32_t lba, amount = size - copied;
            if (amount > 512) amount = 512;
            if (data_sector(cluster, s, &lba) ||
                volume_read(lba, sector_buffer)) return -1;
            for (uint32_t i = 0; i < amount; ++i)
                sector_buffer[i] = ((const uint8_t *)buffer)[copied + i];
            if (volume_write(lba, sector_buffer)) return -1;
            copied += amount;
        }
        if (copied < size) {
            uint32_t next;
            if (fat_next(cluster, &next)) return -1;
            cluster = next;
        }
    }
    return (int)copied;
}

static uint32_t format_name(const struct fat_entry *e, char *out,
                            uint32_t capacity)
{
    uint32_t base = 8, ext = 3, used = 0;
    while (base && e->name[base - 1] == ' ') --base;
    while (ext && e->name[8 + ext - 1] == ' ') --ext;
    for (uint32_t i = 0; i < base && used < capacity; ++i)
        out[used++] = (char)e->name[i];
    if (ext && used < capacity) {
        out[used++] = '.';
        for (uint32_t i = 0; i < ext && used < capacity; ++i)
            out[used++] = (char)e->name[8 + i];
    }
    if ((e->attributes & FAT_ATTR_DIRECTORY) && used < capacity)
        out[used++] = '/';
    return used;
}

static int list_sector(uint32_t lba, char *buffer, uint32_t capacity,
                       uint32_t *used, int *end)
{
    if (volume_read(lba, sector_buffer)) return -1;
    for (uint32_t off = 0; off < 512; off += 32) {
        struct fat_entry *e = (struct fat_entry *)&sector_buffer[off];
        uint32_t n;
        if (!e->name[0]) { *end = 1; return 0; }
        if (e->name[0] == 0xE5 || e->attributes == FAT_ATTR_LFN ||
            (e->attributes & FAT_ATTR_VOLUME)) continue;
        n = format_name(e, buffer + *used, capacity - *used);
        if (!n || n + 1 > capacity - *used) return -22;
        *used += n;
        buffer[(*used)++] = '\n';
    }
    return 0;
}

int fat_list_directory(const char *path, char *buffer, uint32_t capacity)
{
    struct fat_entry e;
    uint32_t cluster, used = 0;
    int root, end = 0, fixed;
    if (!volume.mounted || !buffer || !capacity ||
        resolve(path, &e, &root)) return -1;
    fixed = root && volume.type != FAT32;
    cluster = root ? volume.root_cluster : entry_cluster(&e);
    if (!root && (!(e.attributes & FAT_ATTR_DIRECTORY) ||
                  !valid_cluster(cluster))) return -1;

    if (fixed) {
        for (uint32_t s = 0; s < volume.root_sectors; ++s) {
            int r = list_sector(volume.root_start + s, buffer, capacity,
                                &used, &end);
            if (r || end) return r ? r : (int)used;
        }
        return (int)used;
    }
    for (uint32_t count = 0; count < volume.cluster_count; ++count) {
        uint32_t first = volume.data_start +
                         (cluster - 2) * volume.sectors_per_cluster;
        for (uint32_t s = 0; s < volume.sectors_per_cluster; ++s) {
            int r = list_sector(first + s, buffer, capacity, &used, &end);
            if (r || end) return r ? r : (int)used;
        }
        uint32_t next;
        int r = fat_next(cluster, &next);
        if (r == 1) return (int)used;
        if (r) return -1;
        cluster = next;
    }
    return -1;
}
