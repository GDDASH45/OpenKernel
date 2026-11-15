#include <kernel/ata.h>
#include <fs/sokfs/internal.h>

static int string_length(const char *text, uint32_t *length)
{
    uint32_t n = 0;
    if (text == 0)
        return -1;
    while (text[n] != '\0') {
        if (n >= 255U)
            return -1;
        n++;
    }
    *length = n;
    return 0;
}

static int read_directory(uint32_t inode_number, struct sokfs_inode *inode,
                          struct sokfs_dirent entries[SOKFS_DIRECTORY_ENTRIES])
{
    uint8_t sector[SOKFS_SECTOR_SIZE];
    if (sokfs_internal_read_inode(inode_number, inode) != 0 ||
        inode->type != SOKFS_TYPE_DIRECTORY ||
        inode->start_sector == 0 ||
        ata_read_sector(sokfs_internal_volume_start() + inode->start_sector,
                        sector) != 0)
        return -1;
    for (uint32_t i = 0; i < SOKFS_DIRECTORY_ENTRIES; i++)
        for (uint32_t b = 0; b < SOKFS_ENTRY_SIZE; b++)
            ((uint8_t *)&entries[i])[b] = sector[i * SOKFS_ENTRY_SIZE + b];
    return 0;
}

static int write_directory(uint32_t inode_number,
                           struct sokfs_dirent entries[SOKFS_DIRECTORY_ENTRIES])
{
    struct sokfs_inode inode;
    uint8_t sector[SOKFS_SECTOR_SIZE];
    if (sokfs_internal_read_inode(inode_number, &inode) != 0 ||
        inode->type != SOKFS_TYPE_DIRECTORY)
        return -1;
    for (uint32_t i = 0; i < SOKFS_DIRECTORY_ENTRIES; i++)
        for (uint32_t b = 0; b < SOKFS_ENTRY_SIZE; b++)
            sector[i * SOKFS_ENTRY_SIZE + b] = ((uint8_t *)&entries[i])[b];
    return ata_write_sector(sokfs_internal_volume_start() + inode.start_sector,
                            sector);
}

static int find_child(uint32_t parent, const char *name, uint32_t length,
                      uint32_t *number, uint32_t *type)
{
    struct sokfs_inode directory;
    struct sokfs_dirent entries[SOKFS_DIRECTORY_ENTRIES];
    if (read_directory(parent, &directory, entries) != 0)
        return -1;
    for (uint32_t i = 0; i < SOKFS_DIRECTORY_ENTRIES; i++) {
        uint32_t n = 0;
        if (entries[i].inode == 0)
            continue;
        while (n < sizeof(entries[i].name) && entries[i].name[n] != '\0')
            n++;
        if (n != length)
            continue;
        uint32_t j;
        for (j = 0; j < length && entries[i].name[j] == name[j]; j++)
            ;
        if (j == length) {
            *number = entries[i].inode;
            *type = entries[i].type;
            return (int)i;
        }
    }
    return -1;
}

static int resolve(const char *path, uint32_t *number,
                   struct sokfs_inode *result)
{
    uint32_t current = 0;
    uint32_t position = 0;
    uint32_t length;
    if (path == 0 || path[0] != '/' ||
        string_length(path, &length) != 0 || length == 0)
        return -1;
    if (length == 1) {
        if (sokfs_internal_read_inode(0, result) != 0)
            return -1;
        *number = 0;
        return 0;
    }
    while (position < length) {
        char component[SOKFS_NAME_MAX + 1];
        uint32_t component_length = 0;
        uint32_t child;
        uint32_t type;
        while (position < length && path[position] == '/')
            position++;
        while (position < length && path[position] != '/') {
            if (component_length >= SOKFS_NAME_MAX)
                return -1;
            component[component_length++] = path[position++];
        }
        component[component_length] = '\0';
        if (component_length == 0 ||
            (component_length == 1 && component[0] == '.') ||
            (component_length == 2 && component[0] == '.' &&
             component[1] == '.') ||
            find_child(current, component, component_length, &child, &type) < 0)
            return -1;
        current = child;
        if (position < length && type != SOKFS_TYPE_DIRECTORY)
            return -1;
    }
    if (sokfs_internal_read_inode(current, result) != 0)
        return -1;
    *number = current;
    return 0;
}

static int resolve_parent(const char *path, uint32_t *parent,
                          char name[SOKFS_NAME_MAX + 1], uint32_t *name_length)
{
    uint32_t length;
    uint32_t separator = 0;
    char parent_path[256];
    if (string_length(path, &length) != 0 || length < 2 ||
        path[0] != '/' || path[length - 1] == '/')
        return -1;
    for (uint32_t i = 1; i < length; i++)
        if (path[i] == '/')
            separator = i;
    *name_length = length - separator - 1;
    if (*name_length == 0 || *name_length > SOKFS_NAME_MAX)
        return -1;
    for (uint32_t i = 0; i < *name_length; i++)
        name[i] = path[separator + 1 + i];
    name[*name_length] = '\0';
    if ((*name_length == 1 && name[0] == '.') ||
        (*name_length == 2 && name[0] == '.' && name[1] == '.'))
        return -1;
    if (separator == 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        for (uint32_t i = 0; i < separator; i++)
            parent_path[i] = path[i];
        parent_path[separator] = '\0';
    }
    struct sokfs_inode inode;
    if (resolve(parent_path, parent, &inode) != 0 ||
        inode.type != SOKFS_TYPE_DIRECTORY)
        return -1;
    return 0;
}

int sokfs_fileops_create(const char *path, uint32_t mode, uint32_t type)
{
    struct sokfs_inode parent_inode;
    struct sokfs_inode inode;
    struct sokfs_dirent entries[SOKFS_DIRECTORY_ENTRIES];
    char name[SOKFS_NAME_MAX + 1];
    uint32_t parent;
    uint32_t name_length;
    uint32_t inode_number;
    uint32_t unused_type;
    uint32_t entry_slot = SOKFS_DIRECTORY_ENTRIES;
    uint32_t slot;
    uint8_t zero[SOKFS_SECTOR_SIZE];

    if ((type != SOKFS_TYPE_FILE && type != SOKFS_TYPE_DIRECTORY) ||
        resolve_parent(path, &parent, name, &name_length) != 0 ||
        read_directory(parent, &parent_inode, entries) != 0 ||
        find_child(parent, name, name_length, &inode_number, &unused_type) >= 0)
        return -1;

    for (slot = 1; slot < SOKFS_MAX_INODES; slot++)
        if (sokfs_internal_read_inode(slot, &inode) != 0)
            break;
    if (slot == SOKFS_MAX_INODES)
        return -1;

    for (uint32_t i = 0; i < SOKFS_DIRECTORY_ENTRIES; i++)
        if (entries[i].inode == 0) {
            entry_slot = i;
            break;
        }
    if (entry_slot == SOKFS_DIRECTORY_ENTRIES)
        return -1;

    for (uint32_t i = 0; i < SOKFS_SECTOR_SIZE; i++)
        zero[i] = 0;
    inode.marker = SOKFS_INODE_MARKER;
    inode.type = type;
    inode.mode = mode & 0777U;
    inode.size = 0;
    inode.start_sector = 0;
    inode.sector_count = 0;
    if (type == SOKFS_TYPE_DIRECTORY) {
        if (sokfs_internal_alloc(1, &inode.start_sector) != 0)
            return -1;
        inode.sector_count = 1;
        if (ata_write_sector(sokfs_internal_volume_start() +
                             inode.start_sector, zero) != 0)
            return -1;
    }
    if (sokfs_internal_write_inode(slot, &inode) != 0)
        return -1;

    entries[entry_slot].inode = slot;
    entries[entry_slot].type = type;
    for (uint32_t i = 0; i < sizeof(entries[entry_slot].name); i++)
        entries[entry_slot].name[i] = 0;
    for (uint32_t i = 0; i < name_length; i++)
        entries[entry_slot].name[i] = name[i];
    return write_directory(parent, entries);
}

int sokfs_fileops_read(const char *path, void *buffer, uint32_t capacity)
{
    struct sokfs_inode inode;
    uint32_t number;
    uint8_t sector[SOKFS_SECTOR_SIZE];
    uint8_t *output = (uint8_t *)buffer;
    uint32_t copied = 0;
    if (buffer == 0 || resolve(path, &number, &inode) != 0 ||
        inode.type != SOKFS_TYPE_FILE)
        return -1;
    if (capacity < inode.size)
        return -2;
    while (copied < inode.size) {
        uint32_t amount = inode.size - copied;
        if (amount > SOKFS_SECTOR_SIZE)
            amount = SOKFS_SECTOR_SIZE;
        if (ata_read_sector(sokfs_internal_volume_start() +
                            inode.start_sector + copied / SOKFS_SECTOR_SIZE,
                            sector) != 0)
            return -1;
        for (uint32_t i = 0; i < amount; i++)
            output[copied + i] = sector[i];
        copied += amount;
    }
    return (int)copied;
}

int sokfs_fileops_write(const char *path, const void *buffer, uint32_t size)
{
    struct sokfs_inode inode;
    struct sokfs_inode updated;
    uint32_t number;
    uint32_t sectors = (size + SOKFS_SECTOR_SIZE - 1U) / SOKFS_SECTOR_SIZE;
    uint32_t new_start = 0;
    uint8_t sector[SOKFS_SECTOR_SIZE];
    const uint8_t *input = (const uint8_t *)buffer;
    if ((buffer == 0 && size != 0) || size > 0x7FFFFFFFU ||
        resolve(path, &number, &inode) != 0 ||
        inode.type != SOKFS_TYPE_FILE)
        return -1;

    if (sectors != 0 && sokfs_internal_alloc(sectors, &new_start) != 0)
        return -1;
    for (uint32_t block = 0; block < sectors; block++) {
        for (uint32_t i = 0; i < SOKFS_SECTOR_SIZE; i++) {
            uint32_t offset = block * SOKFS_SECTOR_SIZE + i;
            sector[i] = offset < size ? input[offset] : 0;
        }
        if (ata_write_sector(sokfs_internal_volume_start() +
                             new_start + block, sector) != 0) {
            sokfs_internal_release(new_start, sectors);
            return -1;
        }
    }
    updated.marker = SOKFS_INODE_MARKER;
    updated.type = inode.type;
    updated.mode = inode.mode;
    updated.size = size;
    updated.start_sector = new_start;
    updated.sector_count = sectors;
    for (uint32_t i = 0; i < sizeof(updated.reserved); i++)
        updated.reserved[i] = 0;
    if (sokfs_internal_write_inode(number, &updated) != 0) {
        sokfs_internal_release(new_start, sectors);
        return -1;
    }
    if (inode.sector_count != 0)
        sokfs_internal_release(inode.start_sector, inode.sector_count);
    return (int)size;
}

int sokfs_fileops_list(const char *path, char *buffer, uint32_t capacity)
{
    struct sokfs_inode inode;
    struct sokfs_dirent entries[SOKFS_DIRECTORY_ENTRIES];
    uint32_t number;
    uint32_t used = 0;
    if (buffer == 0 || capacity == 0 ||
        resolve(path, &number, &inode) != 0 ||
        read_directory(number, &inode, entries) != 0)
        return -1;
    for (uint32_t i = 0; i < SOKFS_DIRECTORY_ENTRIES; i++) {
        if (entries[i].inode == 0)
            continue;
        uint32_t length = 0;
        while (length < sizeof(entries[i].name) &&
               entries[i].name[length] != '\0')
            length++;
        if (used + length + 2 > capacity)
            return -2;
        for (uint32_t j = 0; j < length; j++)
            buffer[used++] = entries[i].name[j];
        if (entries[i].type == SOKFS_TYPE_DIRECTORY)
            buffer[used++] = '/';
        buffer[used++] = '\n';
    }
    if (used < capacity)
        buffer[used] = '\0';
    return (int)used;
}

int sokfs_fileops_stat(const char *path, struct sokfs_stat *stat)
{
    struct sokfs_inode inode;
    uint32_t number;
    if (stat == 0 || resolve(path, &number, &inode) != 0)
        return -1;
    stat->inode = number;
    stat->size = inode.size;
    stat->mode = inode.mode;
    stat->type = inode.type;
    return 0;
}
