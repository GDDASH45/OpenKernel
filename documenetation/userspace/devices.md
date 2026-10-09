# Userspace device nodes

Userspace can create VFS device nodes with `ok_mkdev(path, type)` from
`<user/syscall.h>`. Valid type constants are `OK_DEVICE_CONSOLE`,
`OK_DEVICE_NULL`, `OK_DEVICE_ZERO`, `OK_DEVICE_RANDOM`, and
`OK_DEVICE_KEYBOARD` in `<kernel/syscall.h>`.

- Console devices send writes to the active kernel console.
- Null devices discard writes and return end-of-file on reads.
- Zero devices fill read buffers with zero bytes.
- Random devices return a simple pseudo-random byte stream; it is not suitable
  for cryptographic use.
- Keyboard devices return characters already available from the keyboard
  queue.

The init program creates several conventional device paths, including
`/device/console`, `/device/fb0`, `/device/null`, `/device/zero`,
`/device/random`, `/device/keyboard`, and `/device/tty0` through `tty9`. The
VFS itself creates a console and TTY nodes during initialization, and some
requested aliases may already exist or depend on successful path creation.

This API creates namespace entries only. It does not provide general POSIX
file descriptors: the syscall ABI currently has no userspace `open()`,
`read()`, or `close()` operation for these nodes. Framebuffer access is through
the dedicated `ok_fb_*()` syscalls, not `/device/fb0`; see
[Framebuffer API](../kernel/framebuffer.md).
