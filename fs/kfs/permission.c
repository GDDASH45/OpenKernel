
#include <stdint.h>

#include <fs/kfs.h>

/*
 * KFS permission handling
 * OpenKernel
 *
 * Permission bits use the traditional Unix layout:
 *
 *  owner: 0700
 *  group: 0070
 *  other: 0007
 *
 * Requested access:
 *  read    = 4
 *  write   = 2
 *  execute = 1
 */

#define KFS_PERM_EXECUTE  1u
#define KFS_PERM_WRITE    2u
#define KFS_PERM_READ     4u

#define KFS_PERM_OWNER_SHIFT  6u
#define KFS_PERM_GROUP_SHIFT  3u
#define KFS_PERM_OTHER_SHIFT  0u

#define KFS_PERM_MASK         07u

#define KFS_ROOT_UID          0u

static uint32_t
kfs_permission_bits(const struct kfs_inode *inode,
                    uint32_t uid,
                    uint32_t gid)
{
    if (uid == KFS_ROOT_UID)
        return KFS_PERM_MASK;

    if (uid == inode->uid)
        return (inode->permissions >> KFS_PERM_OWNER_SHIFT)
               & KFS_PERM_MASK;

    if (gid == inode->gid)
        return (inode->permissions >> KFS_PERM_GROUP_SHIFT)
               & KFS_PERM_MASK;

    return (inode->permissions >> KFS_PERM_OTHER_SHIFT)
           & KFS_PERM_MASK;
}

/*
 * Return 1 if access is allowed, 0 if denied.
 *
 * requested must contain one or more of:
 * KFS_PERM_READ, KFS_PERM_WRITE, KFS_PERM_EXECUTE.
 */
int
kfs_check_permission(const struct kfs_inode *inode,
                     uint32_t uid,
                     uint32_t gid,
                     uint32_t requested)
{
    uint32_t available;

    if (inode == 0)
        return 0;

    if (requested == 0 || (requested & ~KFS_PERM_MASK) != 0)
        return 0;

    if (inode->flags & KFS_INODE_READONLY) {
        if (requested & KFS_PERM_WRITE)
            return 0;
    }

    available = kfs_permission_bits(inode, uid, gid);

    return (available & requested) == requested;
}
