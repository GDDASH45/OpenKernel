#include <fs/ext4/ext4.h>
#include <kernel/ata.h>
#include <stdint.h>

#define EXT4_SECTOR_SIZE       512u
#define EXT4_SUPER_OFFSET      1024u
#define EXT4_SUPER_MAGIC       0xEF53u
#define EXT4_ROOT_INO          2u
#define EXT4_EXTENTS_FL        0x00080000u
#define EXT4_INODE_MODE_DIR    0x4000u
#define EXT4_INODE_MODE_REG    0x8000u
#define EXT4_FEATURE_INCOMPAT_FILETYPE 0x0002u
#define EXT4_FEATURE_INCOMPAT_EXTENTS  0x0040u
#define EXT4_FEATURE_INCOMPAT_64BIT    0x0080u
#define EXT4_EXTENT_MAGIC      0xF30Au
#define EXT4_MAX_BLOCK_SIZE    4096u
#define EXT4_MAX_PATH          256u
#define EXT4_MAX_DEPTH         5u
#define EXT4_MAX_NAME          255u

struct ext4_state {
    uint32_t partition_lba;
    uint32_t block_size;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t inode_size;
    uint32_t groups;
    uint32_t descriptor_size;
    uint64_t blocks_count;
    uint32_t inodes_count;
    uint64_t descriptor_block;
    uint32_t feature_incompat;
    uint8_t mounted;
};

struct ext4_inode {
    uint16_t mode;
    uint32_t flags;
    uint64_t size;
    uint8_t block[60];
};

static struct ext4_state ext4;
static uint8_t block_buffer[EXT4_MAX_BLOCK_SIZE];
static uint8_t io_buffer[EXT4_MAX_BLOCK_SIZE];

static uint16_t ext4_get16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t ext4_get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t ext4_get64(const uint8_t *p)
{
    return (uint64_t)ext4_get32(p) | ((uint64_t)ext4_get32(p + 4) << 32);
}

static int ext4_read_bytes(uint64_t offset, void *buffer, uint32_t size)
{
    uint8_t *out = (uint8_t *)buffer;
    uint8_t sector[EXT4_SECTOR_SIZE];

    if (!ext4.mounted || (buffer == 0 && size != 0))
        return -1;

    while (size != 0) {
        uint32_t sector_offset = (uint32_t)(offset % EXT4_SECTOR_SIZE);
        uint32_t count = EXT4_SECTOR_SIZE - sector_offset;
        uint64_t lba = offset / EXT4_SECTOR_SIZE;

        if (count > size)
            count = size;
        if (lba > 0xFFFFFFFFu ||
            ext4.partition_lba > 0xFFFFFFFFu - (uint32_t)lba ||
            ata_read_sector(ext4.partition_lba + (uint32_t)lba, sector) != 0)
            return -1;

        for (uint32_t i = 0; i < count; ++i)
            out[i] = sector[sector_offset + i];

        out += count;
        offset += count;
        size -= count;
    }
    return 0;
}

static int ext4_read_block(uint64_t block, void *buffer)
{
    if (block >= ext4.blocks_count ||
        block > (UINT64_MAX / ext4.block_size))
        return -1;
    return ext4_read_bytes(block * ext4.block_size, buffer,
                           ext4.block_size);
}

static int ext4_read_group_descriptor(uint32_t group, uint8_t *descriptor)
{
    uint64_t offset;

    if (group >= ext4.groups)
        return -1;

    offset = ext4.descriptor_block * ext4.block_size +
             (uint64_t)group * ext4.descriptor_size;

    return ext4_read_bytes(offset, descriptor, ext4.descriptor_size);
}

static int ext4_read_inode(uint32_t inode_number, struct ext4_inode *inode)
{
    uint32_t group, index;
    uint8_t descriptor[64];
    uint64_t table_block, inode_offset;
    uint8_t raw[EXT4_MAX_BLOCK_SIZE];

    if (!ext4.mounted || inode == 0 || inode_number == 0 ||
        inode_number > ext4.inodes_count ||
        ext4.descriptor_size > sizeof(descriptor) ||
        ext4.inode_size > sizeof(raw))
        return -1;

    group = (inode_number - 1) / ext4.inodes_per_group;
    index = (inode_number - 1) % ext4.inodes_per_group;

    if (ext4_read_group_descriptor(group, descriptor) != 0)
        return -1;

    table_block = ext4_get32(&descriptor[8]);
    if (ext4.descriptor_size >= 64)
        table_block |= (uint64_t)ext4_get32(&descriptor[40]) << 32;

    inode_offset = table_block * ext4.block_size +
                   (uint64_t)index * ext4.inode_size;

    if (ext4_read_bytes(inode_offset, raw, ext4.inode_size) != 0)
        return -1;

    inode->mode = ext4_get16(raw);
    inode->size = ext4_get32(&raw[4]);
    if (ext4.inode_size >= 112)
        inode->size |= (uint64_t)ext4_get32(&raw[108]) << 32;
    inode->flags = ext4_get32(&raw[32]);

    for (uint32_t i = 0; i < sizeof(inode->block); ++i)
        inode->block[i] = raw[40 + i];

    return 0;
}

/*
 * Resolve one logical file block through the inode's extent tree.
 * Extent index and leaf nodes are read-only; unwritten extents are
 * deliberately rejected rather than returning uninitialized contents.
 */
static int ext4_extent_map(const struct ext4_inode *inode,
                           uint32_t logical, uint64_t *physical)
{
    uint8_t node[EXT4_MAX_BLOCK_SIZE];
    const uint8_t *header;
    uint16_t entries, depth;

    if (!(inode->flags & EXT4_EXTENTS_FL) || physical == 0)
        return -1;

    for (uint32_t i = 0; i < sizeof(node); ++i)
        node[i] = inode->block[i < sizeof(inode->block) ? i : 0];

    header = node;
    if (ext4_get16(header) != EXT4_EXTENT_MAGIC)
        return -1;

    entries = ext4_get16(header + 2);
    depth = ext4_get16(header + 6);
    if (entries > 4)
        return -1;
    if (depth > EXT4_MAX_DEPTH)
        return -1;

    while (depth != 0) {
        uint32_t selected = 0;
        uint32_t selected_key = 0;
        const uint8_t *entry;

        if (entries == 0 || entries > (ext4.block_size - 12) / 12)
            return -1;

        for (uint32_t i = 0; i < entries; ++i) {
            entry = node + 12 + i * 12;
            if (i == 0 || ext4_get32(entry) <= logical) {
                selected = i;
                selected_key = ext4_get32(entry);
            } else {
                break;
            }
        }

        entry = node + 12 + selected * 12;
        if (logical < selected_key)
            return -1;

        {
            uint64_t child = ext4_get32(entry + 4) |
                             ((uint64_t)ext4_get16(entry + 8) << 32);
            if (ext4_read_block(child, node) != 0)
                return -1;
        }

        header = node;
        if (ext4_get16(header) != EXT4_EXTENT_MAGIC)
            return -1;
        entries = ext4_get16(header + 2);
        depth = ext4_get16(header + 6);
        if (depth > EXT4_MAX_DEPTH)
            return -1;
    }

    if (entries > (ext4.block_size - 12) / 12)
        return -1;

    for (uint32_t i = 0; i < entries; ++i) {
        const uint8_t *entry = node + 12 + i * 12;
        uint32_t first = ext4_get32(entry);
        uint16_t raw_length = ext4_get16(entry + 4);
        uint32_t length = raw_length & 0x7FFFu;
        uint64_t start = ext4_get32(entry + 8) |
                         ((uint64_t)ext4_get16(entry + 6) << 32);

        if (raw_length & 0x8000u)
            return -1;
        if (logical >= first && logical - first < length) {
            *physical = start + (logical - first);
            return *physical < ext4.blocks_count ? 0 : -1;
        }
    }

    return -1;
}

static int ext4_read_inode_data(const struct ext4_inode *inode,
                                uint64_t offset, void *buffer,
                                uint32_t size)
{
    uint8_t *out = (uint8_t *)buffer;

    if (inode == 0 || (buffer == 0 && size != 0) ||
        offset > inode->size || size > inode->size - offset)
        return -1;

    while (size != 0) {
        uint32_t logical = (uint32_t)(offset / ext4.block_size);
        uint32_t within = (uint32_t)(offset % ext4.block_size);
        uint32_t count = ext4.block_size - within;
        uint64_t physical;

        if (count > size)
            count = size;
        if (ext4_extent_map(inode, logical, &physical) != 0 ||
            ext4_read_block(physical, block_buffer) != 0)
            return -1;

        for (uint32_t i = 0; i < count; ++i)
            out[i] = block_buffer[within + i];

        out += count;
        offset += count;
        size -= count;
    }

    return 0;
}

static int ext4_lookup_child(uint32_t directory_inode, const char *name,
                             uint32_t name_length, uint32_t *result)
{
    struct ext4_inode directory;
    uint64_t offset = 0;

    if (ext4_read_inode(directory_inode, &directory) != 0 ||
        (directory.mode & 0xF000u) != EXT4_INODE_MODE_DIR)
        return -1;

    while (offset < directory.size) {
        uint8_t entry[EXT4_MAX_NAME + 8];
        uint32_t inode_number, record_length;
        uint8_t entry_name_length;

        if (ext4_read_inode_data(&directory, offset, entry, 8) != 0)
            return -1;

        inode_number = ext4_get32(entry);
        record_length = ext4_get16(entry + 4);
        entry_name_length = entry[6];

        if (record_length < 8 || (record_length & 3u) != 0 ||
            record_length > directory.size - offset ||
            entry_name_length > record_length - 8)
            return -1;

        if (inode_number != 0 && entry_name_length == name_length) {
            if (ext4_read_inode_data(&directory, offset + 8,
                                     entry + 8, entry_name_length) != 0)
                return -1;

            uint32_t i;
            for (i = 0; i < name_length; ++i) {
                if (entry[8 + i] != (uint8_t)name[i])
                    break;
            }
            if (i == name_length) {
                *result = inode_number;
                return 0;
            }
        }

        offset += record_length;
    }

    return -1;
}

static int ext4_resolve(const char *path, struct ext4_inode *inode,
                        uint32_t *inode_number)
{
    uint32_t current = EXT4_ROOT_INO;
    uint32_t position = 0;

    if (path == 0 || path[0] != '/')
        return -1;

    while (path[position] == '/')
        ++position;

    while (path[position] != '\0') {
        char component[EXT4_MAX_NAME + 1];
        uint32_t length = 0;

        while (path[position] != '\0' && path[position] != '/') {
            if (length >= EXT4_MAX_NAME)
                return -1;
            component[length++] = path[position++];
        }
        component[length] = '\0';

        while (path[position] == '/')
            ++position;

        if (length == 0 ||
            ext4_lookup_child(current, component, length, &current) != 0)
            return -1;
    }

    if (ext4_read_inode(current, inode) != 0)
        return -1;
    if (inode_number != 0)
        *inode_number = current;
    return 0;
}

int ext4_mount(uint32_t partition_lba)
{
    uint8_t super[1024];
    uint32_t log_block_size, incompat, first_data_block;
    uint32_t descriptor_size, block_groups, inode_groups;

    ext4.mounted = 0;

    if (partition_lba > 0xFFFFFFFFu - 2u ||
        ata_read_sector(partition_lba + 2u, super) != 0 ||
        ext4_get16(&super[56]) != EXT4_SUPER_MAGIC)
        return -1;

    log_block_size = ext4_get32(&super[24]);
    if (log_block_size > 2)
        return -1;

    ext4.block_size = 1024u << log_block_size;
    ext4.inodes_count = ext4_get32(&super[0]);
    ext4.blocks_count = ext4_get32(&super[4]);
    ext4.blocks_count |= (uint64_t)ext4_get32(&super[336]) << 32;
    ext4.blocks_per_group = ext4_get32(&super[32]);
    ext4.inodes_per_group = ext4_get32(&super[40]);
    ext4.inode_size = ext4_get16(&super[88]);
    incompat = ext4_get32(&super[96]);
    first_data_block = ext4_get32(&super[20]);
    descriptor_size = ext4_get16(&super[254]);

    if (ext4.blocks_count == 0 || ext4.blocks_per_group == 0 ||
        ext4.inodes_per_group == 0 || ext4.inode_size < 128 ||
        ext4.inode_size > ext4.block_size ||
        (ext4.inode_size & (ext4.inode_size - 1)) != 0)
        return -1;

    if (!(incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) ||
        (incompat & ~(EXT4_FEATURE_INCOMPAT_FILETYPE |
                      EXT4_FEATURE_INCOMPAT_EXTENTS |
                      EXT4_FEATURE_INCOMPAT_64BIT)) != 0)
        return -1;

    if (descriptor_size == 0)
        descriptor_size = 32;
    if (descriptor_size < 32 || descriptor_size > 64)
        return -1;

    ext4.partition_lba = partition_lba;
    ext4.feature_incompat = incompat;
    ext4.descriptor_size = descriptor_size;
    if (ext4.blocks_count <= first_data_block || ext4.inodes_count == 0)
        return -1;

    block_groups = (uint32_t)((ext4.blocks_count - first_data_block +
                    ext4.blocks_per_group - 1) / ext4.blocks_per_group);
    inode_groups = (ext4.inodes_count + ext4.inodes_per_group - 1) /
                   ext4.inodes_per_group;
    ext4.groups = block_groups > inode_groups ? block_groups : inode_groups;
    ext4.descriptor_block = first_data_block + 1u;

    if (ext4.groups == 0 || ext4.groups > 0x100000u)
        return -1;

    ext4.mounted = 1;

    {
        struct ext4_inode root;
        if (ext4_read_inode(EXT4_ROOT_INO, &root) != 0 ||
            (root.mode & 0xF000u) != EXT4_INODE_MODE_DIR) {
            ext4.mounted = 0;
            return -1;
        }
    }

    return 0;
}

int ext4_is_mounted(void)
{
    return ext4.mounted != 0;
}

int ext4_read_file(const char *path, void *buffer, uint32_t capacity)
{
    struct ext4_inode inode;

    if (!ext4.mounted || buffer == 0 ||
        ext4_resolve(path, &inode, 0) != 0 ||
        (inode.mode & 0xF000u) != EXT4_INODE_MODE_REG ||
        inode.size > capacity || inode.size > 0xFFFFFFFFu)
        return -1;

    if (ext4_read_inode_data(&inode, 0, buffer, (uint32_t)inode.size) != 0)
        return -1;

    return (int)inode.size;
}

int ext4_list_directory(const char *path, char *buffer, uint32_t capacity)
{
    struct ext4_inode directory;
    uint64_t offset = 0;
    uint32_t used = 0;

    if (!ext4.mounted || buffer == 0 || capacity == 0 ||
        ext4_resolve(path, &directory, 0) != 0 ||
        (directory.mode & 0xF000u) != EXT4_INODE_MODE_DIR)
        return -1;

    while (offset < directory.size) {
        uint8_t entry[EXT4_MAX_NAME + 8];
        uint32_t inode_number, record_length;
        uint8_t name_length;

        if (ext4_read_inode_data(&directory, offset, entry, 8) != 0)
            return -1;

        inode_number = ext4_get32(entry);
        record_length = ext4_get16(entry + 4);
        name_length = entry[6];

        if (record_length < 8 || (record_length & 3u) != 0 ||
            record_length > directory.size - offset ||
            name_length > record_length - 8)
            return -1;

        if (inode_number != 0 && name_length != 0) {
            if (used + name_length + 2 > capacity)
                return -2;
            if (ext4_read_inode_data(&directory, offset + 8,
                                     entry + 8, name_length) != 0)
                return -1;
            for (uint32_t i = 0; i < name_length; ++i)
                buffer[used++] = (char)entry[8 + i];
            buffer[used++] = '\n';
        }

        offset += record_length;
    }

    buffer[used] = '\0';
    return (int)used;
}
