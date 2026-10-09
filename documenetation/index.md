# OpenKernel documentation

OpenKernel is an experimental 32-bit x86 kernel. These pages describe the
implemented interfaces and call out partial or inactive features instead of
assuming conventional operating-system behavior.

## Start here

- [Architecture and current status](kernel/overview.md)
- [Build and run](setup/building.md)
- [Source tree guide](development/source-map.md)

## Boot and storage

- [Boot and startup](kernel/boot.md)
- [Initramfs, TAR, and startup scripts](kernel/initramfs.md)
- [VFS and storage](kernel/storage.md)
- [Userspace device nodes](userspace/devices.md)

## Programs and APIs

- [Userspace system-call ABI](kernel/syscalls.md)
- [Programs and process execution](kernel/processes.md)
- [Userspace init and shell](userspace/shell.md)
- [Framebuffer API](kernel/framebuffer.md)

## Kernel subsystems

- [Display and input](kernel/display-input.md)
- [Sound and timing](kernel/sound-time.md)
- [Interrupts, exceptions, and panic handling](kernel/interrupts.md)
- [Kernel panic API](kernel/panic.md)
- [Memory checks and allocation status](kernel/memory.md)
- [Metadata, assertions, and warnings](kernel/metadata-assertions.md)
- [Kernel module status](kernel/modules.md)
- [sleep_ms reference](time/sleep_ms.md)