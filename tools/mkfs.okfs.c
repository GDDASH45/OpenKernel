#include <fs/okfs/okfs.h>
#include <fs/okfs/okfs2_internal.h>
#include "mkfs_common.h"

#include <limits.h>

int main(int argc, char **argv)
{
    FILE *image;
    uint64_t image_size;
    struct okfs2_superblock super;
    struct okfs2_inode root;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s IMAGE\n", argv[0]);
        return 2;
    }
    if (mkfs_open_image(argv[1], &image, &image_size) != 0)
        return 1;
    if (image_size < ((uint64_t)OKFS2_DATA_START + 2U) * MKFS_SECTOR_SIZE ||
        image_size / MKFS_SECTOR_SIZE > OKFS2_LBA_LIMIT) {
        fprintf(stderr, "mkfs.okfs: image is too small or exceeds the OKFS LBA28 limit\n");
        fclose(image);
        return 1;
    }

    memset(&super, 0, sizeof(super));
    memcpy(super.magic, OKFS2_MAGIC, OKFS_MAGIC_SIZE);
    super.version = OKFS2_VERSION;
    super.root_inode = OKFS2_ROOT_INODE;
    super.inode_head = OKFS2_DATA_START;
    super.inode_tail = OKFS2_DATA_START;
    super.next_lba = OKFS2_DATA_START + 1U;
    super.next_inode_id = OKFS2_ROOT_INODE + 1U;

    memset(&root, 0, sizeof(root));
    root.marker = OKFS2_INODE_MARKER;
    root.inode_id = OKFS2_ROOT_INODE;
    root.parent_id = OKFS2_ROOT_INODE;
    root.mode = 0755U;
    root.type = OKFS2_INODE_DIRECTORY;

    if (mkfs_zero_at(image, (uint64_t)OKFS_SUPER_LBA * MKFS_SECTOR_SIZE,
                     (uint64_t)(OKFS2_DATA_START + 1U - OKFS_SUPER_LBA) *
                     MKFS_SECTOR_SIZE) != 0 ||
        mkfs_write_at(image, (uint64_t)OKFS_SUPER_LBA * MKFS_SECTOR_SIZE,
                      &super, sizeof(super)) != 0 ||
        mkfs_write_at(image, (uint64_t)OKFS2_DATA_START * MKFS_SECTOR_SIZE,
                      &root, sizeof(root)) != 0) {
        fclose(image);
        return 1;
    }

    if (mkfs_finish(image) != 0)
        return 1;
    printf("OKFS2: formatted %s at LBA %u\n", argv[1], OKFS_SUPER_LBA);
    return 0;
}
