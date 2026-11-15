#include <fs/sokfs/sokfs.h>
#include "mkfs_common.h"

#include <limits.h>

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

static void bitmap_set(uint8_t *bitmap, uint32_t sector)
{
    bitmap[sector / 8U] |= (uint8_t)(1U << (sector % 8U));
}

int main(int argc, char **argv)
{
    FILE *image;
    uint64_t image_size;
    uint32_t start_lba = 0;
    uint32_t sector_count;
    uint32_t bitmap_sectors;
    uint32_t root_sector;
    struct sokfs_superblock super;
    struct sokfs_inode root;
    uint8_t *bitmap;
    uint8_t zero[SOKFS_SECTOR_SIZE] = {0};

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s IMAGE [START_LBA [SECTOR_COUNT]]\n", argv[0]);
        return 2;
    }
    if (argc >= 3 && mkfs_parse_u32(argv[2], &start_lba) != 0) {
        fprintf(stderr, "mkfs.sokfs: invalid start LBA\n");
        return 2;
    }
    if (mkfs_open_image(argv[1], &image, &image_size) != 0)
        return 1;
    if (image_size % SOKFS_SECTOR_SIZE != 0 ||
        image_size / SOKFS_SECTOR_SIZE > UINT32_MAX) {
        fprintf(stderr, "mkfs.sokfs: image size must be a whole number of sectors\n");
        fclose(image);
        return 1;
    }

    if (argc == 4) {
        if (mkfs_parse_u32(argv[3], &sector_count) != 0) {
            fprintf(stderr, "mkfs.sokfs: invalid sector count\n");
            fclose(image);
            return 2;
        }
    } else {
        uint64_t available = image_size / SOKFS_SECTOR_SIZE;
        if (start_lba >= available) {
            fprintf(stderr, "mkfs.sokfs: start LBA is outside the image\n");
            fclose(image);
            return 1;
        }
        available -= start_lba;
        if (available > SOKFS_MAX_SECTORS)
            available = SOKFS_MAX_SECTORS;
        sector_count = (uint32_t)available;
    }

    if (sector_count < 512U || sector_count > SOKFS_MAX_SECTORS ||
        start_lba > UINT32_MAX - sector_count ||
        (uint64_t)(start_lba + sector_count) * SOKFS_SECTOR_SIZE > image_size) {
        fprintf(stderr, "mkfs.sokfs: invalid volume range (512..%u sectors supported)\n",
                SOKFS_MAX_SECTORS);
        fclose(image);
        return 1;
    }

    bitmap_sectors = (sector_count + 4095U) / 4096U;
    memset(&super, 0, sizeof(super));
    memcpy(super.magic, SOKFS_MAGIC, sizeof(super.magic));
    super.version = SOKFS_VERSION;
    super.total_sectors = sector_count;
    super.bitmap_sectors = bitmap_sectors;
    super.inode_table_lba = 1U + bitmap_sectors;
    super.data_start = super.inode_table_lba + SOKFS_MAX_INODES;
    super.root_inode = 0;
    root_sector = super.data_start;

    if (root_sector + 1U >= sector_count) {
        fprintf(stderr, "mkfs.sokfs: volume is too small for metadata\n");
        fclose(image);
        return 1;
    }

    memset(&root, 0, sizeof(root));
    root.marker = SOKFS_INODE_MARKER;
    root.type = SOKFS_TYPE_DIRECTORY;
    root.mode = 0755U;
    root.start_sector = root_sector;
    root.sector_count = 1U;

    if (mkfs_zero_at(image, (uint64_t)start_lba * SOKFS_SECTOR_SIZE,
                     (uint64_t)sector_count * SOKFS_SECTOR_SIZE) != 0 ||
        mkfs_write_at(image, (uint64_t)start_lba * SOKFS_SECTOR_SIZE,
                      &super, sizeof(super)) != 0) {
        fclose(image);
        return 1;
    }

    bitmap = calloc(bitmap_sectors, SOKFS_SECTOR_SIZE);
    if (bitmap == NULL) {
        perror("calloc");
        fclose(image);
        return 1;
    }
    for (uint32_t sector = 0; sector <= root_sector; sector++)
        bitmap_set(bitmap, sector);

    if (mkfs_write_at(image,
                      (uint64_t)(start_lba + 1U) * SOKFS_SECTOR_SIZE,
                      bitmap, (size_t)bitmap_sectors * SOKFS_SECTOR_SIZE) != 0) {
        free(bitmap);
        fclose(image);
        return 1;
    }
    free(bitmap);

    if (mkfs_write_at(image,
                      (uint64_t)(start_lba + super.inode_table_lba) *
                      SOKFS_SECTOR_SIZE,
                      &root, sizeof(root)) != 0 ||
        mkfs_write_at(image,
                      (uint64_t)(start_lba + root_sector) *
                      SOKFS_SECTOR_SIZE,
                      zero, sizeof(zero)) != 0) {
        fclose(image);
        return 1;
    }

    if (mkfs_finish(image) != 0)
        return 1;
    printf("SOKFS: formatted %s (LBA %u, %u sectors)\n",
           argv[1], start_lba, sector_count);
    return 0;
}
