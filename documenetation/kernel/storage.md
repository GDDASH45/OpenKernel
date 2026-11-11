# VFS and storage

OpenKernel currently has two distinct storage mechanisms: an in-memory VFS
namespace and an ATA-backed OKFS2 flat-file store. They are not joined by a
mount that makes persistent files visible under VFS paths.

## VFS namespace

The VFS is a fixed-size in-memory tree. It supports absolute-path lookup,
directory creation, file/device nodes, directory listing, and mounts. Limits
in the current implementation are 256 allocated nodes, 16 mounts, and 64
bytes per node name. Startup mounts devtmpfs at `/dev` and as the compatibility
alias `/device`, and mounts generated procfs and sysfs trees at `/proc` and
`/sys`. TAR indexing adds archive paths as nodes, enabling `ls`/directory
lookup; indexed regular files do not currently get TAR-backed read callbacks,
so listing a file does not mean VFS can read its contents.

The `readdir` syscall returns newline-separated child names. Directories have
a trailing `/`. Listing output is limited to the syscall's 1024-byte buffer
maximum.

## Device nodes

Userspace can request device creation with `ok_mkdev(path, type)` from
`<user/syscall.h>`. Device types are defined in `<kernel/syscall.h>`:

- `OK_DEVICE_CONSOLE`: writes go to the active kernel console.
- `OK_DEVICE_NULL`: reads return end-of-file; writes are discarded.
- `OK_DEVICE_ZERO`: reads return zero-filled bytes; writes are discarded.
- `OK_DEVICE_RANDOM`: reads return bytes from a simple linear-congruential
  generator; this is not cryptographically secure.
- `OK_DEVICE_KEYBOARD`: reads consume available translated keyboard characters.

The kernel creates conventional device nodes during VFS initialization; init
does not create them. A created device node is not a general POSIX file
descriptor: the current syscall ABI has no `open`, `read`, or `close`
interface for arbitrary VFS files. `ok_device_read()` and
`ok_device_write()` access a path directly.

## OKFS2 persistent store

OKFS2 uses ATA PIO and a linked inode table with parent-directory identifiers,
so `okfs_read()` and `okfs_write()` accept hierarchical absolute paths. The
number of files and directories is not fixed at 64; entries grow until the
addressable disk space is exhausted. File mode bits are stored and can be
queried or changed, but are not yet enforced. Rewrites append new data rather
than reclaiming the previous allocation, so repeated writes can exhaust disk
space.

On a blank disk, startup formats OKFS2. A disk with the legacy flat OKFS
signature remains readable and writable through the legacy operations, but
hierarchical operations are only available on OKFS2. Startup attempts to
unpack regular files and directories from the initramfs, skipping boot paths,
then writes a few system files. There is currently no delete, rename,
free-space reclamation, reliable disk-capacity detection, or VFS mount
integration. Do not assume data can be listed from the shell merely because
OKFS2 accepted a write.

The implementation is split between the on-disk backend in `fs/okfs/okfs2.c`,
file and path operations in `fs/okfs/fileops.c`, and initramfs extraction in
`fs/okfs/tar_unpack.c`. OKFS headers are in `include/fs/okfs/`.

# OKFS2
The legacy OKFS1 filesystem was removed in OpenKernel 0.2.0-rc3 and is not
part of the current boot or storage documentation. Current references and
startup behavior target the OKFS2 persistent store described above.