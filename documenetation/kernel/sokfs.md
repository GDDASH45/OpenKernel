# SOKFS (SuperOKFS)

SOKFS is a separate, ATA-backed filesystem for OpenKernel. It does not replace
OKFS2 and does not share OKFS2's on-disk format. The first version uses
512-byte sectors and a caller-selected volume starting LBA.

## On-disk layout

- Sector 0: SOKFS superblock (SOKFS001, version 1).
- Following sectors: allocation bitmap, one bit per volume sector.
- Following 128 sectors: inode table, one 512-byte inode per sector.
- Remaining sectors: file and directory data.

The root directory is inode 0. Inodes store type, mode, byte size, and one
contiguous data extent. Directories use one sector containing sixteen
32-byte directory entries. Names are limited to 23 bytes. The volume is
limited to 131072 sectors in this initial format.

## API

Include <fs/sokfs/sokfs.h>:

- sokfs_format(start_lba, sector_count) initializes the selected volume.
  Formatting destroys all data in that range.
- sokfs_mount(start_lba) mounts an existing SOKFS volume.
- sokfs_create(path, mode) creates a regular file.
- sokfs_mkdir(path, mode) creates a directory.
- sokfs_read(path, buffer, capacity) reads a complete file.
- sokfs_write(path, buffer, size) replaces a file's contents.
- sokfs_list(path, buffer, capacity) lists a directory as newline-separated
  names; directories have a trailing slash.
- sokfs_stat(path, stat) retrieves inode number, size, mode, and type.

Paths are absolute. Parent directories must already exist. File writes use a
new contiguous extent and release the previous extent after the inode update.
The driver does not automatically format or mount a disk at boot.

## Current limitations

This is an initial standalone filesystem implementation. It is not yet
integrated with the VFS, file-descriptor syscalls, or initramfs extraction.
Directories are limited to sixteen entries and one data sector. Each file uses
one contiguous extent, so fragmentation can prevent large allocations even
when enough free sectors remain in total. There is no delete/rename operation,
permission enforcement, journaling, fsck, or automatic partition discovery.
The caller must supply a valid volume range and ensure it does not overlap
other filesystems or disk metadata.
