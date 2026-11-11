#include <kernel/ata.h>
#include <fs/okfs/fileops.h>
#include <fs/okfs/okfs2_internal.h>

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

static int find_inode_by_id(const struct okfs2_superblock *superblock,
                            uint32_t inode_id,
                            struct okfs2_inode *result,
                            uint32_t *result_lba) {
    uint32_t lba = superblock->inode_head;
    uint32_t limit = okfs2_inode_scan_limit(superblock);

    for (uint32_t count = 0; lba != 0 && count < limit; count++) {
        struct okfs2_inode inode;

        if (lba >= superblock->next_lba ||
            okfs2_read_inode(lba, &inode) != 0) {
            return -1;
        }
        if (inode.inode_id == inode_id) {
            *result = inode;
            *result_lba = lba;
            return 0;
        }
        lba = inode.next_lba;
    }

    return -1;
}

static int find_child(const struct okfs2_superblock *superblock,
                      uint32_t parent_id,
                      const char *name,
                      uint32_t name_length,
                      struct okfs2_inode *result,
                      uint32_t *result_lba) {
    uint32_t lba = superblock->inode_head;
    uint32_t limit = okfs2_inode_scan_limit(superblock);

    for (uint32_t count = 0; lba != 0 && count < limit; count++) {
        struct okfs2_inode inode;

        if (lba >= superblock->next_lba ||
            okfs2_read_inode(lba, &inode) != 0) {
            return -1;
        }
        if (inode.parent_id == parent_id &&
            inode.name_length == name_length &&
            bytes_equal(inode.name, name, name_length)) {
            *result = inode;
            *result_lba = lba;
            return 0;
        }
        lba = inode.next_lba;
    }

    return -1;
}

static int resolve_path(const struct okfs2_superblock *superblock,
                        const char *path,
                        struct okfs2_inode *result,
                        uint32_t *result_lba) {
    uint32_t parent_id = OKFS2_ROOT_INODE;
    uint32_t position = 1;
    uint32_t path_length = 0;

    if (path == 0 || path[0] != '/') {
        return -1;
    }
    while (path[path_length] != '\0') {
        if (++path_length > OKFS_PATH_MAX) {
            return -1;
        }
    }
    if (path_length == 1) {
        return find_inode_by_id(superblock, OKFS2_ROOT_INODE,
                                result, result_lba);
    }

    while (position < path_length) {
        uint32_t start = position;
        uint32_t component_length;

        while (position < path_length && path[position] != '/') {
            position++;
        }
        component_length = position - start;
        if (component_length == 0 || component_length > OKFS2_NAME_MAX ||
            (component_length == 1 && path[start] == '.') ||
            (component_length == 2 && path[start] == '.' &&
             path[start + 1] == '.')) {
            return -1;
        }
        if (find_child(superblock, parent_id, path + start,
                       component_length, result, result_lba) != 0) {
            return -1;
        }
        if (position < path_length) {
            if (result->type != OKFS2_INODE_DIRECTORY) {
                return -1;
            }
            parent_id = result->inode_id;
            position++;
            if (position == path_length) {
                return -1;
            }
        }
    }

    return 0;
}

static int resolve_parent(const struct okfs2_superblock *superblock,
                          const char *path,
                          struct okfs2_inode *parent,
                          const char **name,
                          uint32_t *name_length) {
    uint32_t length = 0;
    uint32_t separator = 0;
    uint32_t unused_lba;
    char parent_path[OKFS_PATH_MAX + 1];

    if (path == 0 || path[0] != '/') {
        return -1;
    }
    while (path[length] != '\0') {
        if (++length > OKFS_PATH_MAX) {
            return -1;
        }
    }
    if (length < 2 || path[length - 1] == '/') {
        return -1;
    }
    for (uint32_t index = 1; index < length; index++) {
        if (path[index] == '/') {
            if (index == separator + 1) {
                return -1;
            }
            separator = index;
        }
    }

    *name = path + separator + 1;
    *name_length = length - separator - 1;
    if (*name_length == 0 || *name_length > OKFS2_NAME_MAX ||
        (*name_length == 1 && (*name)[0] == '.') ||
        (*name_length == 2 && (*name)[0] == '.' && (*name)[1] == '.')) {
        return -1;
    }

    if (separator == 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        for (uint32_t index = 0; index < separator; index++) {
            parent_path[index] = path[index];
        }
        parent_path[separator] = '\0';
    }
    if (resolve_path(superblock, parent_path, parent, &unused_lba) != 0 ||
        parent->type != OKFS2_INODE_DIRECTORY) {
        return -1;
    }

    return 0;
}

static int create_inode(struct okfs2_superblock *superblock,
                        uint32_t parent_id,
                        const char *name,
                        uint32_t name_length,
                        uint32_t type,
                        uint32_t mode,
                        struct okfs2_inode *result,
                        uint32_t *result_lba) {
    struct okfs2_inode parent;
    struct okfs2_inode existing;
    struct okfs2_inode inode;
    uint32_t unused_lba;
    uint32_t new_lba;
    uint32_t tail_lba;
    uint32_t current_lba;
    uint32_t limit;

    if (find_inode_by_id(superblock, parent_id, &parent, &unused_lba) != 0 ||
        parent.type != OKFS2_INODE_DIRECTORY ||
        find_child(superblock, parent_id, name, name_length,
                   &existing, &unused_lba) == 0 ||
        superblock->next_inode_id == 0xFFFFFFFFU ||
        okfs2_reserve_sector(superblock, &new_lba) != 0) {
        return -1;
    }

    bytes_zero(&inode, sizeof(inode));
    inode.marker = OKFS2_INODE_MARKER;
    inode.inode_id = superblock->next_inode_id++;
    inode.parent_id = parent_id;
    inode.mode = mode & 0777U;
    inode.type = type;
    inode.name_length = name_length;
    for (uint32_t index = 0; index < name_length; index++) {
        inode.name[index] = name[index];
    }

    if (okfs2_save_superblock(superblock) != 0 ||
        okfs2_write_inode(new_lba, &inode) != 0) {
        return -1;
    }

    current_lba = superblock->inode_head;
    limit = okfs2_inode_scan_limit(superblock);
    tail_lba = 0;
    for (uint32_t count = 0; current_lba != 0 && count < limit; count++) {
        struct okfs2_inode current;

        if (current_lba >= superblock->next_lba ||
            okfs2_read_inode(current_lba, &current) != 0) {
            return -1;
        }
        if (current.next_lba == 0) {
            tail_lba = current_lba;
            break;
        }
        current_lba = current.next_lba;
    }
    if (tail_lba == 0) {
        return -1;
    }

    {
        struct okfs2_inode tail;

        if (okfs2_read_inode(tail_lba, &tail) != 0) {
            return -1;
        }
        tail.next_lba = new_lba;
        if (okfs2_write_inode(tail_lba, &tail) != 0) {
            return -1;
        }
    }

    superblock->inode_tail = new_lba;
    if (okfs2_save_superblock(superblock) != 0) {
        return -1;
    }
    if (result != 0) {
        *result = inode;
    }
    if (result_lba != 0) {
        *result_lba = new_lba;
    }

    return 0;
}

static int create_path(struct okfs2_superblock *superblock,
                       const char *path,
                       uint32_t type,
                       uint32_t mode,
                       struct okfs2_inode *result,
                       uint32_t *result_lba) {
    struct okfs2_inode parent;
    const char *name;
    uint32_t name_length;

    if (resolve_parent(superblock, path, &parent, &name, &name_length) != 0) {
        return -1;
    }
    return create_inode(superblock, parent.inode_id, name, name_length,
                        type, mode, result, result_lba);
}

static int allocate_file_data(struct okfs2_superblock *superblock,
                              const uint8_t *data,
                              uint32_t size,
                              uint32_t *data_lba) {
    uint16_t sector[256];
    uint32_t sectors = (size + 511U) / 512U;

    *data_lba = 0;
    for (uint32_t index = 0; index < sectors; index++) {
        uint32_t lba;

        if (okfs2_reserve_sector(superblock, &lba) != 0) {
            return -1;
        }
        for (uint32_t byte = 0; byte < 512; byte++) {
            uint32_t source_index = index * 512U + byte;
            ((uint8_t *)sector)[byte] =
                source_index < size ? data[source_index] : 0;
        }
        if (ata_write_sector(lba, sector) != 0) {
            return -1;
        }
        if (index == 0) {
            *data_lba = lba;
        }
    }

    return 0;
}

int okfs_fileops_read(const char *path, void *buffer, uint32_t capacity) {
    struct okfs2_superblock superblock;
    struct okfs2_inode inode;
    uint32_t inode_lba;
    uint16_t sector[256];
    uint8_t *destination = (uint8_t *)buffer;
    uint32_t copied = 0;

    if (!okfs2_is_mounted() || buffer == 0 ||
        okfs2_load_superblock(&superblock) != 0 ||
        resolve_path(&superblock, path, &inode, &inode_lba) != 0 ||
        inode.type != OKFS2_INODE_FILE) {
        return -1;
    }
    if (capacity < inode.size) {
        return -2;
    }
    if (inode.size != 0) {
        if (inode.size > 0xFFFFFFFFU - 511U) {
            return -1;
        }
        uint32_t sectors = (inode.size + 511U) / 512U;
        if (inode.data_lba < OKFS2_DATA_START ||
            inode.data_lba >= superblock.next_lba ||
            sectors > superblock.next_lba - inode.data_lba) {
            return -1;
        }
    }

    while (copied < inode.size) {
        uint32_t amount = inode.size - copied;
        if (amount > 512) {
            amount = 512;
        }
        if (ata_read_sector(inode.data_lba + copied / 512U, sector) != 0) {
            return -1;
        }
        for (uint32_t byte = 0; byte < amount; byte++) {
            destination[copied + byte] = ((uint8_t *)sector)[byte];
        }
        copied += amount;
    }

    return (int)copied;
}

int okfs_fileops_write(const char *path, const void *data, uint32_t size) {
    struct okfs2_superblock superblock;
    struct okfs2_inode inode;
    uint32_t inode_lba;
    uint32_t data_lba;

    if (!okfs2_is_mounted() || (data == 0 && size != 0) ||
        size > 0x7FFFFFFFU ||
        okfs2_load_superblock(&superblock) != 0) {
        return -1;
    }
    if (resolve_path(&superblock, path, &inode, &inode_lba) != 0) {
        if (create_path(&superblock, path, OKFS2_INODE_FILE, 0644,
                        &inode, &inode_lba) != 0 ||
            okfs2_load_superblock(&superblock) != 0) {
            return -1;
        }
    } else if (inode.type != OKFS2_INODE_FILE) {
        return -1;
    }

    if (allocate_file_data(&superblock, (const uint8_t *)data,
                           size, &data_lba) != 0) {
        return -1;
    }
    inode.data_lba = data_lba;
    inode.size = size;
    return okfs2_write_inode(inode_lba, &inode) == 0 ? (int)size : -1;
}

int okfs_fileops_create(const char *path, uint32_t mode) {
    struct okfs2_superblock superblock;

    if (!okfs2_is_mounted() ||
        okfs2_load_superblock(&superblock) != 0) {
        return -1;
    }
    return create_path(&superblock, path, OKFS2_INODE_FILE,
                       mode, 0, 0);
}

int okfs_fileops_mkdir(const char *path, uint32_t mode) {
    struct okfs2_superblock superblock;
    struct okfs2_inode existing;
    uint32_t existing_lba;

    if (!okfs2_is_mounted() ||
        okfs2_load_superblock(&superblock) != 0) {
        return -1;
    }
    if (resolve_path(&superblock, path, &existing, &existing_lba) == 0) {
        return existing.type == OKFS2_INODE_DIRECTORY ? 0 : -1;
    }
    return create_path(&superblock, path, OKFS2_INODE_DIRECTORY,
                       mode, 0, 0);
}

int okfs_fileops_chmod(const char *path, uint32_t mode) {
    struct okfs2_superblock superblock;
    struct okfs2_inode inode;
    uint32_t inode_lba;

    if (!okfs2_is_mounted() ||
        okfs2_load_superblock(&superblock) != 0 ||
        resolve_path(&superblock, path, &inode, &inode_lba) != 0) {
        return -1;
    }
    inode.mode = mode & 0777U;
    return okfs2_write_inode(inode_lba, &inode);
}

int okfs_fileops_stat(const char *path, struct okfs_stat *stat) {
    struct okfs2_superblock superblock;
    struct okfs2_inode inode;
    uint32_t inode_lba;

    if (!okfs2_is_mounted() || stat == 0 ||
        okfs2_load_superblock(&superblock) != 0 ||
        resolve_path(&superblock, path, &inode, &inode_lba) != 0) {
        return -1;
    }
    stat->inode = inode.inode_id;
    stat->size = inode.size;
    stat->mode = inode.mode;
    stat->type = inode.type;
    return 0;
}
