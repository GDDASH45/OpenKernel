#include <kernel/ata.h>
#include <fs/okfs/okfs2.h>
#include <fs/okfs/okfs2_internal.h>

typedef char okfs2_superblock_must_be_one_sector[
    sizeof(struct okfs2_superblock) == 512 ? 1 : -1];
typedef char okfs2_inode_must_be_one_sector[
    sizeof(struct okfs2_inode) == 512 ? 1 : -1];

static int mounted;

static int bytes_equal(const char *left, const char *right, uint32_t length) {
    for (uint32_t index = 0; index < length; index++) {
        if (left[index] != right[index]) {
            return 0;
        }
    }

    return 1;
}

static void bytes_zero(void *buffer, uint32_t length) {
    uint8_t *bytes = (uint8_t *)buffer;

    for (uint32_t index = 0; index < length; index++) {
        bytes[index] = 0;
    }
}

int okfs2_is_mounted(void) {
    return mounted;
}

int okfs2_load_superblock(struct okfs2_superblock *superblock) {
    if (superblock == 0 ||
        ata_read_sector(OKFS_SUPER_LBA, superblock) != 0 ||
        !bytes_equal(superblock->magic, OKFS2_MAGIC, OKFS_MAGIC_SIZE) ||
        superblock->version != OKFS2_VERSION ||
        superblock->root_inode != OKFS2_ROOT_INODE ||
        superblock->inode_head < OKFS2_DATA_START ||
        superblock->inode_tail < OKFS2_DATA_START ||
        superblock->next_lba <= OKFS2_DATA_START ||
        superblock->next_lba > OKFS2_LBA_LIMIT ||
        superblock->inode_head >= superblock->next_lba ||
        superblock->inode_tail >= superblock->next_lba ||
        superblock->next_inode_id <= OKFS2_ROOT_INODE) {
        return -1;
    }

    return 0;
}

int okfs2_save_superblock(const struct okfs2_superblock *superblock) {
    if (superblock == 0) {
        return -1;
    }
    return ata_write_sector(OKFS_SUPER_LBA, superblock);
}

int okfs2_read_inode(uint32_t lba, struct okfs2_inode *inode) {
    if (inode == 0 || lba < OKFS2_DATA_START ||
        ata_read_sector(lba, inode) != 0 ||
        inode->marker != OKFS2_INODE_MARKER ||
        inode->inode_id == 0 ||
        inode->name_length > OKFS2_NAME_MAX ||
        (inode->type != OKFS2_INODE_FILE &&
         inode->type != OKFS2_INODE_DIRECTORY)) {
        return -1;
    }

    return 0;
}

int okfs2_write_inode(uint32_t lba, const struct okfs2_inode *inode) {
    if (inode == 0 || lba < OKFS2_DATA_START ||
        lba >= OKFS2_LBA_LIMIT) {
        return -1;
    }
    return ata_write_sector(lba, inode);
}

int okfs2_reserve_sector(struct okfs2_superblock *superblock,
                         uint32_t *reserved_lba) {
    if (superblock == 0 || reserved_lba == 0 ||
        superblock->next_lba >= OKFS2_LBA_LIMIT) {
        return -1;
    }

    *reserved_lba = superblock->next_lba++;
    return okfs2_save_superblock(superblock);
}

uint32_t okfs2_inode_scan_limit(const struct okfs2_superblock *superblock) {
    if (superblock == 0 || superblock->next_lba < OKFS2_DATA_START) {
        return 0;
    }
    return superblock->next_lba - OKFS2_DATA_START;
}

int okfs2_format(void) {
    struct okfs2_superblock superblock;
    struct okfs2_inode root;

    bytes_zero(&superblock, sizeof(superblock));
    for (uint32_t index = 0; index < OKFS_MAGIC_SIZE; index++) {
        superblock.magic[index] = OKFS2_MAGIC[index];
    }
    superblock.version = OKFS2_VERSION;
    superblock.root_inode = OKFS2_ROOT_INODE;
    superblock.inode_head = OKFS2_DATA_START;
    superblock.inode_tail = OKFS2_DATA_START;
    superblock.next_lba = OKFS2_DATA_START + 1;
    superblock.next_inode_id = OKFS2_ROOT_INODE + 1;

    bytes_zero(&root, sizeof(root));
    root.marker = OKFS2_INODE_MARKER;
    root.inode_id = OKFS2_ROOT_INODE;
    root.parent_id = OKFS2_ROOT_INODE;
    root.mode = 0755;
    root.type = OKFS2_INODE_DIRECTORY;

    if (okfs2_save_superblock(&superblock) != 0 ||
        okfs2_write_inode(OKFS2_DATA_START, &root) != 0) {
        mounted = 0;
        return -1;
    }

    mounted = 1;
    return 0;
}

int okfs2_mount(void) {
    struct okfs2_superblock superblock;
    struct okfs2_inode root;
    uint32_t root_lba = 0;
    uint32_t current_lba;
    uint32_t limit;
    int found_tail = 0;

    mounted = 0;
    if (okfs2_load_superblock(&superblock) != 0) {
        return -1;
    }

    current_lba = superblock.inode_head;
    limit = okfs2_inode_scan_limit(&superblock);
    for (uint32_t count = 0; current_lba != 0 && count < limit; count++) {
        if (current_lba >= superblock.next_lba ||
            okfs2_read_inode(current_lba, &root) != 0) {
            return -1;
        }
        if (root.inode_id == OKFS2_ROOT_INODE) {
            root_lba = current_lba;
            found_tail = root.type == OKFS2_INODE_DIRECTORY;
        }
        if (root.next_lba == 0) {
            if (current_lba != superblock.inode_tail) {
                return -1;
            }
            break;
        }
        current_lba = root.next_lba;
    }

    if (!found_tail || current_lba == 0 || root.next_lba != 0 ||
        root_lba != OKFS2_DATA_START) {
        return -1;
    }

    mounted = 1;
    return 0;
}
