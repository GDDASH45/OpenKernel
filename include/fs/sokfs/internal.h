#ifndef INCLUDE_FS_SOKFS_INTERNAL_H
#define INCLUDE_FS_SOKFS_INTERNAL_H

#include <fs/sokfs/sokfs.h>

#define SOKFS_INODE_MARKER 0x534F4B49U
#define SOKFS_DIRECTORY_ENTRIES 16U
#define SOKFS_ENTRY_SIZE 32U

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

struct sokfs_dirent {
    uint32_t inode;
    uint32_t type;
    char name[24];
} __attribute__((packed));

uint32_t sokfs_internal_volume_start(void);
struct sokfs_superblock *sokfs_internal_super(void);
int sokfs_internal_read_inode(uint32_t number, struct sokfs_inode *inode);
int sokfs_internal_write_inode(uint32_t number, const struct sokfs_inode *inode);
int sokfs_internal_alloc(uint32_t count, uint32_t *first);
int sokfs_internal_release(uint32_t first, uint32_t count);

#endif
