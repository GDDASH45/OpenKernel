
#ifndef OPENKERNEL_FS_KFS_H
#define OPENKERNEL_FS_KFS_H

#include <stdint.h>
#include <stddef.h>

/*
 * KFS - Kernel File System
 * OpenKernel
 *
 * Initial on-disk format and filesystem interface.
 */

/* Filesystem identification */
#define KFS_MAGIC           0x4B465331u  /* "KFS1" */
#define KFS_VERSION          1u

/* Block layout */
#define KFS_BLOCK_SIZE       4096u
#define KFS_ROOT_INODE       1u
#define KFS_NAME_MAX         255u
#define KFS_DIRECT_BLOCKS    12u

/* Inode types */
#define KFS_TYPE_FILE        1u
#define KFS_TYPE_DIRECTORY  2u
#define KFS_TYPE_SYMLINK     3u

/* Inode flags */
#define KFS_INODE_ALLOCATED  (1u << 0)
#define KFS_INODE_READONLY   (1u << 1)

/*
 * On-disk superblock.
 *
 * Stored at filesystem block 0.
 * All fields are fixed-width to keep the format predictable.
 */
struct kfs_superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;

    uint32_t inode_table_start;
    uint32_t inode_count;
    uint32_t inode_bitmap_start;
    uint32_t block_bitmap_start;

    uint32_t data_block_start;
    uint32_t root_inode;
    uint32_t flags;
    uint32_t checksum;

    uint8_t reserved[64];
};

/*
 * On-disk inode.
 *
 * Direct block pointers refer to filesystem block numbers.
 * Size is measured in bytes.
 */
struct kfs_inode {
    uint32_t inode_number;
    uint32_t type;
    uint32_t flags;
    uint32_t permissions;

    uint64_t size;
    uint64_t created_time;
    uint64_t modified_time;

    uint32_t link_count;
    uint32_t direct_blocks[KFS_DIRECT_BLOCKS];

    uint32_t indirect_block;
    uint32_t reserved[7];
    uint32_t uid;
    uint32_t gid;
};

/*
 * On-disk directory entry.
 *
 * name_length specifies how many bytes of name are valid.
 * Names are not required to fill the entire name array.
 */
struct kfs_dir_entry {
    uint32_t inode_number;
    uint16_t name_length;
    uint8_t type;
    uint8_t reserved;
    char name[KFS_NAME_MAX + 1];
};

/*
 * Runtime filesystem state.
 *
 * device_id is owned by the storage layer.
 * block_count is the size of the filesystem device in blocks.
 */
struct kfs_fs {
    uint32_t device_id;
    uint32_t block_count;
    struct kfs_superblock superblock;
    int mounted;
};

/* Block-device callbacks. Return 0 on success. */
typedef int (*kfs_read_block_fn)(
    uint32_t device_id,
    uint32_t block_number,
    void *buffer
);

typedef int (*kfs_write_block_fn)(
    uint32_t device_id,
    uint32_t block_number,
    const void *buffer
);

/* Filesystem initialization and mount operations. */
int kfs_init(void);

int kfs_mount(
    struct kfs_fs *fs,
    uint32_t device_id,
    uint32_t block_count,
    kfs_read_block_fn read_block,
    kfs_write_block_fn write_block
);

int kfs_unmount(struct kfs_fs *fs);

/* Inode operations. */
int kfs_read_inode(
    struct kfs_fs *fs,
    uint32_t inode_number,
    struct kfs_inode *inode
);

int kfs_write_inode(
    struct kfs_fs *fs,
    const struct kfs_inode *inode
);

/* File data operations. */
int kfs_read(
    struct kfs_fs *fs,
    uint32_t inode_number,
    uint64_t offset,
    void *buffer,
    size_t size,
    size_t *bytes_read
);

int kfs_write(
    struct kfs_fs *fs,
    uint32_t inode_number,
    uint64_t offset,
    const void *buffer,
    size_t size,
    size_t *bytes_written
);

/* Directory lookup. */
int kfs_lookup(
    struct kfs_fs *fs,
    uint32_t directory_inode,
    const char *name,
    uint32_t *result_inode
);

int kfs_check_permission(
    const struct kfs_inode *inode,
    uint32_t uid,
    uint32_t gid,
    uint32_t requested
);

int kfs_block_register(
    struct kfs_fs *fs,
    kfs_read_block_fn read_block,
    kfs_write_block_fn write_block
);

int kfs_block_unregister(struct kfs_fs *fs);

int kfs_read_block(
    struct kfs_fs *fs,
    uint32_t block_number,
    void *buffer
);

int kfs_write_block(
    struct kfs_fs *fs,
    uint32_t block_number,
    const void *buffer
);

#endif /* OPENKERNEL_FS_KFS_H */
