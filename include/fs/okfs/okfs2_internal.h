#ifndef INCLUDE_FS_OKFS_OKFS2_INTERNAL_H
#define INCLUDE_FS_OKFS_OKFS2_INTERNAL_H

#include <fs/okfs/okfs.h>

#define OKFS2_MAGIC "OKFS002"
#define OKFS2_VERSION 2
#define OKFS2_ROOT_INODE 1
#define OKFS2_DATA_START (OKFS_DIRECTORY_LBA + OKFS_DIRECTORY_SECTORS)
#define OKFS2_LBA_LIMIT 0x10000000U
#define OKFS2_INODE_MARKER 0x32464B4FU
#define OKFS2_INODE_FILE OKFS_TYPE_FILE
#define OKFS2_INODE_DIRECTORY OKFS_TYPE_DIRECTORY
#define OKFS2_NAME_MAX 100

struct okfs2_superblock {
    char magic[OKFS_MAGIC_SIZE];
    uint32_t version;
    uint32_t root_inode;
    uint32_t inode_head;
    uint32_t inode_tail;
    uint32_t next_lba;
    uint32_t next_inode_id;
    uint8_t reserved[480];
} __attribute__((packed));

struct okfs2_inode {
    uint32_t marker;
    uint32_t inode_id;
    uint32_t parent_id;
    uint32_t next_lba;
    uint32_t data_lba;
    uint32_t size;
    uint32_t mode;
    uint32_t type;
    uint32_t name_length;
    char name[OKFS2_NAME_MAX];
    uint8_t reserved[376];
} __attribute__((packed));

int okfs2_is_mounted(void);
int okfs2_load_superblock(struct okfs2_superblock *superblock);
int okfs2_save_superblock(const struct okfs2_superblock *superblock);
int okfs2_read_inode(uint32_t lba, struct okfs2_inode *inode);
int okfs2_write_inode(uint32_t lba, const struct okfs2_inode *inode);
int okfs2_reserve_sector(struct okfs2_superblock *superblock,
                         uint32_t *reserved_lba);
uint32_t okfs2_inode_scan_limit(const struct okfs2_superblock *superblock);

#endif
