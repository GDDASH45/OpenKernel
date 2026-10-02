#include "vfs.h"
#include <write/write.h>

#define MAX_VFS_NODES 256
static struct vfs_node node_pool[MAX_VFS_NODES];
static uint32_t node_count = 0;
static struct vfs_node root_node = { "/", VFS_DIRECTORY, 0, 0, 0, 0, 0, 0, 0 };

static void vfs_reset_node(struct vfs_node *node) {
    if (node == 0) {
        return;
    }
    node->name[0] = '\0';
    node->flags = 0;
    node->parent = 0;
    node->children = 0;
    node->next = 0;
    node->read = 0;
    node->write = 0;
    node->data = 0;
    node->size = 0;
}

static int vfs_name_is_valid(const char *name) {
    if (name == 0 || name[0] == '\0') {
        return 0;
    }
    for (int index = 0; name[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
        if (name[index] == '/' && index != 0) {
            return 0;
        }
    }
    return 1;
}

static const char *vfs_next_path_component(const char *path, char *component, uint32_t component_size) {
    uint32_t index = 0;
    uint32_t component_index = 0;

    while (path[index] == '/') {
        index++;
    }
    while (path[index] != '\0' && path[index] != '/' && component_index + 1 < component_size) {
        component[component_index++] = path[index++];
    }
    component[component_index] = '\0';
    while (path[index] == '/') {
        index++;
    }
    return path + index;
}

struct vfs_node *vfs_get_root(void) {
    return &root_node;
}

struct vfs_node *vfs_create_node(const char *name, uint32_t flags, struct vfs_node *parent) {
    struct vfs_node *node;
    int index = 0;

    if (node_count >= MAX_VFS_NODES || !vfs_name_is_valid(name) || parent == 0) {
        return 0;
    }

    node = &node_pool[node_count++];
    vfs_reset_node(node);
    while (name[index] != '\0' && index < VFS_NAME_SIZE - 1) {
        node->name[index] = name[index];
        index++;
    }
    node->name[index] = '\0';
    node->flags = flags;
    node->parent = parent;
    node->children = 0;
    node->next = parent->children;
    parent->children = node;
    return node;
}

struct vfs_node *vfs_lookup(struct vfs_node *root, const char *path) {
    struct vfs_node *current = root;
    char component[VFS_NAME_SIZE];
    const char *rest = path;

    if (root == 0 || path == 0 || path[0] != '/') {
        return 0;
    }

    while (*rest != '\0') {
        rest = vfs_next_path_component(rest, component, sizeof(component));
        if (component[0] == '\0') {
            break;
        }

        struct vfs_node *child = current->children;
        int found = 0;
        while (child != 0) {
            if (vfs_name_is_valid(child->name) && child->name[0] == component[0] &&
                (child->name[1] == '\0' || child->name[1] == component[1])) {
                int match = 1;
                for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                    if (child->name[index] != component[index]) {
                        match = 0;
                        break;
                    }
                }
                if (match) {
                    current = child;
                    found = 1;
                    break;
                }
            }
            child = child->next;
        }
        if (!found) {
            return 0;
        }
    }
    return current;
}

struct vfs_node *vfs_mkdir(struct vfs_node *root, const char *path) {
    struct vfs_node *parent = root;
    char component[VFS_NAME_SIZE];
    const char *rest = path;

    if (root == 0 || path == 0 || path[0] != '/') {
        return 0;
    }

    while (*rest != '\0') {
        rest = vfs_next_path_component(rest, component, sizeof(component));
        if (component[0] == '\0') {
            break;
        }
        if (rest[0] == '\0') {
            struct vfs_node *child = parent->children;
            while (child != 0) {
                if (child->name[0] == component[0] && child->flags & VFS_DIRECTORY) {
                    int match = 1;
                    for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                        if (child->name[index] != component[index]) {
                            match = 0;
                            break;
                        }
                    }
                    if (match) {
                        return child;
                    }
                }
                child = child->next;
            }
            return vfs_create_node(component, VFS_DIRECTORY, parent);
        }

        struct vfs_node *child = parent->children;
        int found = 0;
        while (child != 0) {
            int match = 1;
            for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                if (child->name[index] != component[index]) {
                    match = 0;
                    break;
                }
            }
            if (match && (child->flags & VFS_DIRECTORY)) {
                parent = child;
                found = 1;
                break;
            }
            child = child->next;
        }
        if (!found) {
            struct vfs_node *new_dir = vfs_create_node(component, VFS_DIRECTORY, parent);
            if (new_dir == 0) {
                return 0;
            }
            parent = new_dir;
        }
    }

    return parent;
}

struct vfs_node *vfs_create_file(struct vfs_node *root, const char *path,
                                int (*reader)(struct vfs_node *node, char *buf, uint32_t size),
                                int (*writer)(struct vfs_node *node, const char *buf, uint32_t size)) {
    struct vfs_node *parent = root;
    char component[VFS_NAME_SIZE];
    const char *rest = path;
    struct vfs_node *file = 0;

    if (root == 0 || path == 0 || path[0] != '/') {
        return 0;
    }

    while (*rest != '\0') {
        rest = vfs_next_path_component(rest, component, sizeof(component));
        if (component[0] == '\0') {
            break;
        }
        if (rest[0] == '\0') {
            struct vfs_node *node = parent->children;
            while (node != 0) {
                if (node->name[0] == component[0] && node->name[1] == component[1]) {
                    int match = 1;
                    for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                        if (node->name[index] != component[index]) {
                            match = 0;
                            break;
                        }
                    }
                    if (match) {
                        return node;
                    }
                }
                node = node->next;
            }
            file = vfs_create_node(component, VFS_FILE, parent);
            if (file != 0) {
                file->read = reader;
                file->write = writer;
                file->data = 0;
            }
            return file;
        }

        struct vfs_node *child = parent->children;
        int found = 0;
        while (child != 0) {
            int match = 1;
            for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                if (child->name[index] != component[index]) {
                    match = 0;
                    break;
                }
            }
            if (match && (child->flags & VFS_DIRECTORY)) {
                parent = child;
                found = 1;
                break;
            }
            child = child->next;
        }
        if (!found) {
            parent = vfs_create_node(component, VFS_DIRECTORY, parent);
            if (parent == 0) {
                return 0;
            }
        }
    }
    return file;
}

int vfs_mount_fs(struct vfs_node *parent, const char *name, struct vfs_node *root,
                 int (*reader)(const char *path, void *buf, uint32_t size),
                 int (*writer)(const char *path, const void *buf, uint32_t size)) {
    struct vfs_node *mount_node;

    if (parent == 0 || name == 0 || root == 0) {
        return -1;
    }

    mount_node = vfs_create_node(name, VFS_DIRECTORY | VFS_MOUNTED, parent);
    if (mount_node == 0) {
        return -1;
    }
    mount_node->data = root;
    mount_node->read = 0;
    mount_node->write = 0;
    (void)reader;
    (void)writer;
    return 0;
}

int vfs_exists(struct vfs_node *root, const char *path) {
    return vfs_lookup(root, path) != 0;
}

int vfs_read_file(struct vfs_node *root, const char *path, char *buf, uint32_t size) {
    struct vfs_node *node = vfs_lookup(root, path);

    if (node == 0 || buf == 0 || node->read == 0) {
        return -1;
    }
    return node->read(node, buf, size);
}

int vfs_write_file(struct vfs_node *root, const char *path, const char *buf, uint32_t size) {
    struct vfs_node *node = vfs_lookup(root, path);

    if (node == 0 || buf == 0 || node->write == 0) {
        return -1;
    }
    return node->write(node, buf, size);
}

void mount_essential_folders(void) {
    vfs_mkdir(vfs_get_root(), "/etc");
    vfs_mkdir(vfs_get_root(), "/home");
    vfs_mkdir(vfs_get_root(), "/var");
    vfs_mkdir(vfs_get_root(), "/usr");
    vfs_mkdir(vfs_get_root(), "/dev");
    vfs_mkdir(vfs_get_root(), "/sys");
    vfs_mkdir(vfs_get_root(), "/proc");
    k_print("[VFS] Essential directories initialized.\n");
}

void vfs_init(void) {
    static int initialised = 0;

    if (initialised) {
        return;
    }
    initialised = 1;
    root_node.parent = 0;
    root_node.children = 0;
    root_node.next = 0;
    root_node.flags = VFS_DIRECTORY;
    root_node.name[0] = '/';
    root_node.name[1] = '\0';
    mount_essential_folders();
}