
#include <stdint.h>
#include <stddef.h>

#include <fs/kfs.h>

/*
 * KFS mount management
 * OpenKernel
 *
 * Validates the on-disk superblock and initializes
 * the runtime filesystem state.
 */

static kfs_read_block_fn kfs_mount_read_block;
static kfs_write_block_fn kfs_mount_write_block;

/*
 * Check whether a block number is inside the device.
 */
static int
kfs_block_valid(uint32_t block_number, uint32_t block_count)
{
    return block_number < block_count;
}

/*
 * Validate the KFS superblock.
 *
 * This performs structural checks only. A checksum algorithm
 * must be defined before the checksum field can be verified.
 */
static int
kfs_validate_superblock(const struct kfs_superblock *sb,
                        uint32_t device_blocks)
{
    if (sb == NULL)
        return -1;

    if (sb->magic != KFS_MAGIC)
        return -1;

    if (sb->version != KFS_VERSION)
        return -1;

    if (sb->block_size != KFS_BLOCK_SIZE)
        return -1;

    if (sb->total_blocks == 0 ||
        sb->total_blocks > device_blocks)
        return -1;

    if (sb->inode_count == 0)
        return -1;

    if (sb->root_inode != KFS_ROOT_INODE)
        return -1;

    if (sb->inode_table_start >= sb->total_blocks)
        return -1;

    if (sb->inode_bitmap_start >= sb->total_blocks)
        return -1;

    if (sb->block_bitmap_start >= sb->total_blocks)
        return -1;

    if (sb->data_block_start >= sb->total_blocks)
        return -1;

    if (sb->inode_table_start >= sb->data_block_start ||
        sb->inode_bitmap_start >= sb->data_block_start ||
        sb->block_bitmap_start >= sb->data_block_start)
        return -1;

    if (!kfs_block_valid(0, sb->total_blocks))
        return -1;

    return 0;
}

/*
 * Mount an existing KFS filesystem.
 *
 * read_block and write_block must operate on exactly
 * KFS_BLOCK_SIZE bytes per block.
 *
 * Returns 0 on success, -1 on failure.
 */
int
kfs_mount(struct kfs_fs *fs,
          uint32_t device_id,
          uint32_t block_count,
          kfs_read_block_fn read_block,
          kfs_write_block_fn write_block)
{
    struct kfs_superblock sb;
    uint8_t block[KFS_BLOCK_SIZE];

    if (fs == NULL ||
        read_block == NULL ||
        write_block == NULL ||
        block_count == 0)
        return -1;

    if (fs->mounted)
        return -1;

    if (read_block(device_id, 0, block) != 0)
        return -1;

    /*
     * Copy the superblock out of the raw block buffer.
     */
    {
        size_t i;
        uint8_t *dst = (uint8_t *)&sb;
        const uint8_t *src = block;

        for (i = 0; i < sizeof(sb); ++i)
            dst[i] = src[i];
    }

    if (kfs_validate_superblock(&sb, block_count) != 0)
        return -1;

    fs->device_id = device_id;
    fs->block_count = sb.total_blocks;
    fs->superblock = sb;

    kfs_mount_read_block = read_block;
    kfs_mount_write_block = write_block;

    fs->mounted = 1;

    return 0;
}

/*
 * Unmount KFS.
 *
 * This initial implementation has no write-back cache,
 * journal, or dirty metadata to flush.
 */
int
kfs_unmount(struct kfs_fs *fs)
{
    if (fs == NULL || !fs->mounted)
        return -1;

    fs->mounted = 0;
    fs->device_id = 0;
    fs->block_count = 0;

    return 0;
}

/*
 * Internal helpers for other KFS source files.
 *
 * These can later be moved into kfs_block.c as the
 * filesystem's block I/O layer is implemented.
 */
int
kfs_mount_read(uint32_t device_id,
               uint32_t block_number,
               void *buffer)
{
    if (kfs_mount_read_block == NULL || buffer == NULL)
        return -1;

    return kfs_mount_read_block(device_id, block_number, buffer);
}

int
kfs_mount_write(uint32_t device_id,
                uint32_t block_number,
                const void *buffer)
{
    if (kfs_mount_write_block == NULL || buffer == NULL)
        return -1;

    return kfs_mount_write_block(device_id, block_number, buffer);
}
