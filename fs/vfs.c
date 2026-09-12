#include "vfs.h"
#include <write/write.h>

#define MAX_VFS_NODES 128
static struct vfs_node node_pool[MAX_VFS_NODES];
static uint32_t node_count = 0;

static struct vfs_node root_node = { "/", VFS_DIRECTORY, 0, 0, 0, 0, 0 };

struct vfs_node* vfs_get_root(void) {
    return &root_node;
}

struct vfs_node* vfs_create_node(const char *name, uint32_t flags, struct vfs_node *parent) {
    if (node_count >= MAX_VFS_NODES) {
        return 0; // Pool exhausted
    }

    struct vfs_node *node = &node_pool[node_count++];

    int i = 0;
    while (name[i] && i < 63) {
        node->name[i] = name[i];
        i++;
    }
    node->name[i] = '\0';
    
    node->flags = flags;
    node->parent = parent;
    node->children = 0;
    node->next = 0;
    node->read = 0;
    node->write = 0;

    // Link the new node into the parent's child list
    if (parent) {
        node->next = parent->children;
        parent->children = node;
    }

    return node;
}

void mount_essential_folders(void) {
#ifdef DEVDIR
    struct vfs_node *dev_dir = vfs_create_node("device", VFS_DIRECTORY, &root_node);
#endif
    vfs_create_node("sys", VFS_DIRECTORY, &root_node);
    k_print("mounted essential folders!\n");
#ifdef ENABLE_FB
    struct vfs_node *fb_node = vfs_create_node("fb0", VFS_DEVICE, dev_dir);
    // fb_node->write = fb_write_handler;
#endif
}

void vfs_init(void) {
    mount_essential_folders();
}