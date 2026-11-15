#include <fs/kfs.h>
#include "mkfs_common.h"

#include <limits.h>

static void set_bit(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8U] |= (uint8_t)(1U << (bit % 8U));
}

int main(int argc, char **argv)
{
    FILE *image;
    uint64_t image_size;
    uint32_t inode_count = 128U;
    uint32_t total_blocks;
    uint32_t inode_table_blocks;
    uint32_t inode_bitmap_blocks;
    uint32_t block_bitmap_blocks;
    uint32_t inode_bitmap_start;
    uint32_t block_bitmap_start;
    uint32_t data_start;
    uint32_t root_data_block;
    uint8_t *bitmap;
    struct kfs_superblock super;
    struct kfs_inode root;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s IMAGE [INODE_COUNT]\n", argv[0]);
        return 2;
    }
    if (argc == 3 && (mkfs_parse_u32(argv[2], &inode_count) != 0 ||
                      inode_count <= KFS_ROOT_INODE)) {
        fprintf(stderr, "mkfs.kfs: invalid inode count\n");
        return 2;
    }
    if (mkfs_open_image(argv[1], &image, &image_size) != 0)
        return 1;
    if (image_size < KFS_BLOCK_SIZE * 8ULL ||
        image_size % KFS_BLOCK_SIZE != 0 ||
        image_size / KFS_BLOCK_SIZE > UINT32_MAX) {
        fprintf(stderr, "mkfs.kfs: image must be at least 8 blocks and a multiple of %u bytes\n",
                KFS_BLOCK_SIZE);
        fclose(image);
        return 1;
    }

    total_blocks = (uint32_t)(image_size / KFS_BLOCK_SIZE);
    inode_table_blocks =
        (uint32_t)(((uint64_t)inode_count * sizeof(struct kfs_inode) +
                    KFS_BLOCK_SIZE - 1U) / KFS_BLOCK_SIZE);
    inode_bitmap_blocks = (inode_count + KFS_BLOCK_SIZE * 8U - 1U) /
                          (KFS_BLOCK_SIZE * 8U);
    block_bitmap_blocks = (total_blocks + KFS_BLOCK_SIZE * 8U - 1U) /
                          (KFS_BLOCK_SIZE * 8U);
    inode_bitmap_start = 1U + inode_table_blocks;
    block_bitmap_start = inode_bitmap_start + inode_bitmap_blocks;
    data_start = block_bitmap_start + block_bitmap_blocks;
    root_data_block = data_start;

    if (root_data_block + 1U >= total_blocks) {
        fprintf(stderr, "mkfs.kfs: image is too small for requested inode count\n");
        fclose(image);
        return 1;
    }

    memset(&super, 0, sizeof(super));
    super.magic = KFS_MAGIC;
    super.version = KFS_VERSION;
    super.block_size = KFS_BLOCK_SIZE;
    super.total_blocks = total_blocks;
    super.inode_table_start = 1U;
    super.inode_count = inode_count;
    super.inode_bitmap_start = inode_bitmap_start;
    super.block_bitmap_start = block_bitmap_start;
    super.data_block_start = data_start;
    super.root_inode = KFS_ROOT_INODE;

    memset(&root, 0, sizeof(root));
    root.inode_number = KFS_ROOT_INODE;
    root.type = KFS_TYPE_DIRECTORY;
    root.flags = KFS_INODE_ALLOCATED;
    root.permissions = 0755U;
    root.link_count = 1U;
    root.direct_blocks[0] = root_data_block;

    if (mkfs_zero_at(image, 0, image_size) != 0 ||
        mkfs_write_at(image, 0, &super, sizeof(super)) != 0 ||
        mkfs_write_at(image, (uint64_t)KFS_BLOCK_SIZE +
                      (uint64_t)KFS_ROOT_INODE * sizeof(root),
                      &root, sizeof(root)) != 0) {
        fclose(image);
        return 1;
    }

    bitmap = calloc(inode_bitmap_blocks, KFS_BLOCK_SIZE);
    if (bitmap == NULL) {
        perror("calloc");
        fclose(image);
        return 1;
    }
    set_bit(bitmap, KFS_ROOT_INODE);
    if (mkfs_write_at(image, (uint64_t)inode_bitmap_start * KFS_BLOCK_SIZE,
                      bitmap, (size_t)inode_bitmap_blocks * KFS_BLOCK_SIZE) != 0) {
        free(bitmap);
        fclose(image);
        return 1;
    }
    free(bitmap);

    bitmap = calloc(block_bitmap_blocks, KFS_BLOCK_SIZE);
    if (bitmap == NULL) {
        perror("calloc");
        fclose(image);
        return 1;
    }
    for (uint32_t block_number = 0; block_number <= root_data_block;
         block_number++)
        set_bit(bitmap, block_number);
    if (mkfs_write_at(image, (uint64_t)block_bitmap_start * KFS_BLOCK_SIZE,
                      bitmap, (size_t)block_bitmap_blocks * KFS_BLOCK_SIZE) != 0) {
        free(bitmap);
        fclose(image);
        return 1;
    }
    free(bitmap);

    if (mkfs_finish(image) != 0)
        return 1;
    printf("KFS: formatted %s (%u blocks, %u inodes)\n",
           argv[1], total_blocks, inode_count);
    return 0;
}
