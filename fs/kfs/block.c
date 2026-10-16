
#include <stdint.h>
#include <stddef.h>

#include <fs/kfs.h>

/*
 * KFS block I/O
 * OpenKernel
 *
 * Provides checked filesystem-block reads and writes.
 * Each operation transfers exactly KFS_BLOCK_SIZE bytes.
 */

struct kfs_block_device {
    kfs_read_block_fn read_block;
    kfs_write_block_fn write_block;
};

/*
 * The initial KFS interface does not yet store callbacks
 * in struct kfs_fs. Keep a small registry for mounted
 * filesystem instances.
 *
 * This can be replaced with callback fields in struct
 * kfs_fs when the public interface is updated.
 */
#define KFS_MAX_MOUNTS 8u

static struct {
    struct kfs_fs *fs;
    struct kfs_block_device device;
} kfs_devices[KFS_MAX_MOUNTS];

/*
 * Register the device callbacks for a mounted filesystem.
 */
int
kfs_block_register(struct kfs_fs *fs,
                   kfs_read_block_fn read_block,
                   kfs_write_block_fn write_block)
{
    uint32_t i;

    if (fs == NULL || read_block == NULL || write_block == NULL)
        return -1;

    for (i = 0; i < KFS_MAX_MOUNTS; ++i) {
        if (kfs_devices[i].fs == fs)
            return -1;
    }

    for (i = 0; i < KFS_MAX_MOUNTS; ++i) {
        if (kfs_devices[i].fs == NULL) {
            kfs_devices[i].fs = fs;
            kfs_devices[i].device.read_block = read_block;
            kfs_devices[i].device.write_block = write_block;
            return 0;
        }
    }

    return -1;
}

/*
 * Remove a filesystem's callback registration.
 */
int
kfs_block_unregister(struct kfs_fs *fs)
{
    uint32_t i;

    if (fs == NULL)
        return -1;

    for (i = 0; i < KFS_MAX_MOUNTS; ++i) {
        if (kfs_devices[i].fs == fs) {
            kfs_devices[i].fs = NULL;
            kfs_devices[i].device.read_block = NULL;
            kfs_devices[i].device.write_block = NULL;
            return 0;
        }
    }

    return -1;
}

/*
 * Find the registered device callbacks for a filesystem.
 */
static struct kfs_block_device *
kfs_block_get_device(struct kfs_fs *fs)
{
    uint32_t i;

    for (i = 0; i < KFS_MAX_MOUNTS; ++i) {
        if (kfs_devices[i].fs == fs)
            return &kfs_devices[i].device;
    }

    return NULL;
}

/*
 * Read one filesystem block.
 *
 * Returns 0 on success, -1 on failure.
 */
int
kfs_read_block(struct kfs_fs *fs,
               uint32_t block_number,
               void *buffer)
{
    struct kfs_block_device *device;

    if (fs == NULL || buffer == NULL || !fs->mounted)
        return -1;

    if (block_number >= fs->block_count)
        return -1;

    device = kfs_block_get_device(fs);

    if (device == NULL || device->read_block == NULL)
        return -1;

    return device->read_block(
        fs->device_id,
        block_number,
        buffer
    );
}

/*
 * Write one filesystem block.
 *
 * Returns 0 on success, -1 on failure.
 */
int
kfs_write_block(struct kfs_fs *fs,
                uint32_t block_number,
                const void *buffer)
{
    struct kfs_block_device *device;

    if (fs == NULL || buffer == NULL || !fs->mounted)
        return -1;

    if (block_number >= fs->block_count)
        return -1;

    if (fs->superblock.flags & KFS_INODE_READONLY)
        return -1;

    device = kfs_block_get_device(fs);

    if (device == NULL || device->write_block == NULL)
        return -1;

    return device->write_block(
        fs->device_id,
        block_number,
        buffer
    );
}
