#include <kernel/ata.h>
#include <fs/sokfs/sokfs.h>

#define SOKFS_MAGIC "SOKFS001"
#define SOKFS_VERSION 1U
#define SOKFS_INODE_MARKER 0x534F4B49U

struct sokfs_superblock {
    char magic[8];
    uint32_t version;
    uint32_t total_sectors;
    uint32_t bitmap_sectors;
    uint32_t inode_table_lba;
    uint32_t data_start;
    uint32_t root_inode;
    uint8_t reserved[480];
} __attribute__((packed));

struct sokfs_inode {
    uint32_t marker;
    uint32_t type;
    uint32_t mode;
    uint32_t size;
    uint32_t start_sector;
    uint32_t sector_count;
    uint8_t reserved[488];
} __attribute__((packed));

static uint32_t volume_start;
static struct sokfs_superblock mounted_super;
static int mounted;

typedef char sokfs_superblock_size_check[
    sizeof(struct sokfs_superblock) == SOKFS_SECTOR_SIZE ? 1 : -1];
typedef char sokfs_inode_size_check[
    sizeof(struct sokfs_inode) == SOKFS_SECTOR_SIZE ? 1 : -1];

static void bytes_zero(void *buffer, uint32_t size)
{
    uint8_t *bytes = (uint8_t *)buffer;
    for (uint32_t i = 0; i < size; i++)
        bytes[i] = 0;
}

static int bytes_equal(const char *a, const char *b, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static int read_inode(uint32_t number, struct sokfs_inode *inode)
{
    if (number >= SOKFS_MAX_INODES || inode == 0)
        return -1;
    if (ata_read_sector(volume_start + mounted_super.inode_table_lba +
                        number, inode) != 0)
        return -1;
    if (inode->marker != SOKFS_INODE_MARKER ||
        (inode->type != SOKFS_TYPE_FILE &&
         inode->type != SOKFS_TYPE_DIRECTORY))
        return -1;
    if (inode->type == SOKFS_TYPE_DIRECTORY) {
        if (inode->start_sector < mounted_super.data_start ||
            inode->start_sector >= mounted_super.total_sectors ||
            inode->sector_count != 1)
            return -1;
    } else if (inode->size != 0) {
        if (inode->sector_count == 0 ||
            inode->start_sector < mounted_super.data_start ||
            inode->start_sector >= mounted_super.total_sectors ||
            inode->sector_count > mounted_super.total_sectors - inode->start_sector ||
            inode->size > inode->sector_count * SOKFS_SECTOR_SIZE)
            return -1;
    }
    return 0;
}

static int write_inode(uint32_t number, const struct sokfs_inode *inode)
{
    if (number >= SOKFS_MAX_INODES || inode == 0)
        return -1;
    return ata_write_sector(volume_start + mounted_super.inode_table_lba +
                            number, inode);
}

static int bitmap_set(uint32_t sector, int used)
{
    uint8_t bitmap[SOKFS_SECTOR_SIZE];
    uint32_t bitmap_sector = sector / 4096U;
    uint32_t bit = sector % 4096U;
    uint8_t mask = (uint8_t)(1U << (bit % 8U));

    if (sector >= mounted_super.total_sectors ||
        bitmap_sector >= mounted_super.bitmap_sectors ||
        ata_read_sector(volume_start + 1U + bitmap_sector, bitmap) != 0)
        return -1;
    if (used)
        bitmap[bit / 8U] |= mask;
    else
        bitmap[bit / 8U] &= (uint8_t)~mask;
    return ata_write_sector(volume_start + 1U + bitmap_sector, bitmap);
}

static int bitmap_get(uint32_t sector, int *used)
{
    uint8_t bitmap[SOKFS_SECTOR_SIZE];
    uint32_t bitmap_sector = sector / 4096U;
    uint32_t bit = sector % 4096U;

    if (sector >= mounted_super.total_sectors ||
        bitmap_sector >= mounted_super.bitmap_sectors ||
        ata_read_sector(volume_start + 1U + bitmap_sector, bitmap) != 0)
        return -1;
    *used = (bitmap[bit / 8U] & (1U << (bit % 8U))) != 0;
    return 0;
}

static int allocate_run(uint32_t count, uint32_t *first)
{
    uint32_t run = 0;
    uint32_t start = 0;

    if (count == 0) {
        *first = 0;
        return 0;
    }
    for (uint32_t sector = mounted_super.data_start;
         sector < mounted_super.total_sectors; sector++) {
        int used;
        if (bitmap_get(sector, &used) != 0)
            return -1;
        if (!used) {
            if (run == 0)
                start = sector;
            run++;
            if (run == count) {
                for (uint32_t i = 0; i < count; i++)
                    if (bitmap_set(start + i, 1) != 0)
                        return -1;
                *first = start;
                return 0;
            }
        } else {
            run = 0;
        }
    }
    return -1;
}

static int release_run(uint32_t first, uint32_t count)
{
    if (count == 0)
        return 0;
    if (first < mounted_super.data_start ||
        first >= mounted_super.total_sectors ||
        count > mounted_super.total_sectors - first)
        return -1;
    for (uint32_t i = 0; i < count; i++)
        if (bitmap_set(first + i, 0) != 0)
            return -1;
    return 0;
}

int sokfs_format(uint32_t start_lba, uint32_t sector_count)
{
    struct sokfs_superblock super;
    struct sokfs_inode root;
    uint8_t zero[SOKFS_SECTOR_SIZE];
    uint32_t bitmap_sectors;

    mounted = 0;
    if (sector_count < 512U || sector_count > SOKFS_MAX_SECTORS ||
        start_lba > 0xFFFFFFFFU - sector_count)
        return -1;
    bitmap_sectors = (sector_count + 4095U) / 4096U;

    bytes_zero(&super, sizeof(super));
    for (uint32_t i = 0; i < 8; i++)
        super.magic[i] = SOKFS_MAGIC[i];
    super.version = SOKFS_VERSION;
    super.total_sectors = sector_count;
    super.bitmap_sectors = bitmap_sectors;
    super.inode_table_lba = 1U + bitmap_sectors;
    super.data_start = super.inode_table_lba + SOKFS_MAX_INODES;
    super.root_inode = 0;

    if (super.data_start + 1U >= sector_count)
        return -1;

    bytes_zero(zero, sizeof(zero));
    volume_start = start_lba;
    if (ata_write_sector(volume_start, &super) != 0)
        return -1;

    for (uint32_t i = 0; i < bitmap_sectors; i++)
        if (ata_write_sector(volume_start + 1U + i, zero) != 0)
            return -1;

    for (uint32_t i = 0; i < SOKFS_MAX_INODES; i++)
        if (ata_write_sector(volume_start + super.inode_table_lba + i,
                             zero) != 0)
            return -1;

    mounted_super = super;
    mounted = 1;

    for (uint32_t i = 0; i < super.data_start; i++)
        if (bitmap_set(i, 1) != 0) {
            mounted = 0;
            return -1;
        }

    bytes_zero(&root, sizeof(root));
    root.marker = SOKFS_INODE_MARKER;
    root.type = SOKFS_TYPE_DIRECTORY;
    root.mode = 0755U;
    if (allocate_run(1, &root.start_sector) != 0 ||
        ata_write_sector(volume_start + root.start_sector, zero) != 0) {
        mounted = 0;
        return -1;
    }
    root.sector_count = 1;
    if (write_inode(0, &root) != 0) {
        mounted = 0;
        return -1;
    }
    return 0;
}

int sokfs_mount(uint32_t start_lba)
{
    struct sokfs_superblock super;
    struct sokfs_inode root;
    uint32_t minimum;

    mounted = 0;
    volume_start = start_lba;
    if (ata_read_sector(volume_start, &super) != 0 ||
        !bytes_equal(super.magic, SOKFS_MAGIC, 8) ||
        super.version != SOKFS_VERSION ||
        super.total_sectors < 512U ||
        super.total_sectors > SOKFS_MAX_SECTORS ||
        start_lba > 0xFFFFFFFFU - super.total_sectors)
        return -1;

    minimum = 1U + super.bitmap_sectors + SOKFS_MAX_INODES;
    if (super.bitmap_sectors != (super.total_sectors + 4095U) / 4096U ||
        super.inode_table_lba != 1U + super.bitmap_sectors ||
        super.data_start != minimum ||
        super.data_start >= super.total_sectors ||
        super.root_inode != 0)
        return -1;

    mounted_super = super;
    if (read_inode(0, &root) != 0 ||
        root.type != SOKFS_TYPE_DIRECTORY ||
        root.start_sector < super.data_start ||
        root.start_sector >= super.total_sectors ||
        root.sector_count != 1)
        return -1;

    mounted = 1;
    return 0;
}

int sokfs_is_mounted(void)
{
    return mounted;
}

extern int sokfs_fileops_create(const char *, uint32_t, uint32_t);
extern int sokfs_fileops_read(const char *, void *, uint32_t);
extern int sokfs_fileops_write(const char *, const void *, uint32_t);
extern int sokfs_fileops_list(const char *, char *, uint32_t);
extern int sokfs_fileops_stat(const char *, struct sokfs_stat *);

int sokfs_create(const char *path, uint32_t mode)
{
    return mounted ? sokfs_fileops_create(path, mode, SOKFS_TYPE_FILE) : -1;
}

int sokfs_mkdir(const char *path, uint32_t mode)
{
    return mounted ? sokfs_fileops_create(path, mode, SOKFS_TYPE_DIRECTORY) : -1;
}

int sokfs_read(const char *path, void *buffer, uint32_t capacity)
{
    return mounted ? sokfs_fileops_read(path, buffer, capacity) : -1;
}

int sokfs_write(const char *path, const void *buffer, uint32_t size)
{
    return mounted ? sokfs_fileops_write(path, buffer, size) : -1;
}

int sokfs_list(const char *path, char *buffer, uint32_t capacity)
{
    return mounted ? sokfs_fileops_list(path, buffer, capacity) : -1;
}

int sokfs_stat(const char *path, struct sokfs_stat *stat)
{
    return mounted ? sokfs_fileops_stat(path, stat) : -1;
}

uint32_t sokfs_internal_volume_start(void) { return volume_start; }
struct sokfs_superblock *sokfs_internal_super(void) { return &mounted_super; }
int sokfs_internal_read_inode(uint32_t n, struct sokfs_inode *i) { return read_inode(n, i); }
int sokfs_internal_write_inode(uint32_t n, const struct sokfs_inode *i) { return write_inode(n, i); }
int sokfs_internal_alloc(uint32_t n, uint32_t *first) { return allocate_run(n, first); }
int sokfs_internal_release(uint32_t first, uint32_t n) { return release_run(first, n); }
