#ifndef VFS_H
#define VFS_H

#include <stdint.h>

#define VFS_DIRECTORY 0x01
#define VFS_DEVICE    0x02

struct vfs_node {
    char name[64];
    uint32_t flags;
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;
    int (*read)(char *buf, uint32_t size);
    int (*write)(const char *buf, uint32_t size);
};

void vfs_init(void);
struct vfs_node* vfs_create_node(const char *name, uint32_t flags, struct vfs_node *parent);
void mount_essential_folders(void);

#endif