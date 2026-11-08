# Boot and startup

## Boot contract

GRUB loads the 32-bit Multiboot loader and an initramfs module. The loader
inflates the embedded gzip-compressed plain-kernel ELF, loads its segments at
their linked addresses, then transfers the original Multiboot handoff to the
kernel entry code, which establishes the kernel stack and calls
`kernel_main()`.

The kernel is linked at 1 MiB. The current implementation expects x86
protected-mode Multiboot information and initializes hardware directly. It is
not the ARM port: `arch/arm/boot.s` is not part of the normal Makefile assembly
source selection.

## Initialization sequence

At a high level, startup:

1. Decompresses and validates the embedded kernel ELF, then loads it at its
   linked address (8 MiB).
2. Reads Multiboot information, builds the physical-frame bitmap from the
   memory map, and enables the x86 identity-mapped paging directory.
3. Reads the Multiboot command line and initializes boot options.
4. Initializes the IDT/PIC and PS/2 keyboard.
5. Initializes the VFS, mounts devtmpfs, procfs, and sysfs, and selects the
   console.
6. Initializes the Multiboot framebuffer when available; attempts mouse setup.
7. Obtains the first GRUB module as the initramfs, indexes its TAR paths, and
   inserts supported ELF32 modules from `/lib/modules/`.
8. Mounts/formats OKFS when ATA storage is available and attempts to unpack
   regular archive files.
9. Registers the initramfs base for program loading, initializes sound and
   other startup components, then executes the configured init entry.

The decompressor and loader are implemented in `loader/loader.c`; kernel
startup and error handling are implemented in `kernel/start.c`. The loader
stops with an on-screen error if the gzip stream or embedded ELF is invalid.
Some optional devices log failures and allow startup to continue; required
boot metadata, keyboard setup, invalid console options, and missing required
TAR top-level directories cause panics.

## Boot options

Options are whitespace-separated tokens in the GRUB kernel command line:

- `quiet` suppresses output routed through `k_print()`; it does not suppress
  all direct character/device output.
- `init=PATH` selects the TAR entry to execute. The default is `sys/init`.
- `console=PATH` chooses an existing writable device. The default is
  `/device/console`. `/device/tty` followed by digits is accepted as an alias
  for the base console.

The init path buffer holds at most 127 characters and the console path at most
31. Options are not shell-parsed: quoting and spaces inside values are not
supported. Invalid or oversized values are not a mechanism for constructing
arbitrary paths; failed copies leave the default/current option intact.

## Init selection

The kernel looks up `init=PATH` directly in the initramfs TAR by entry name.
The configured entry may be a supported text init script or a recognized
program image. See [Initramfs and init scripts](initramfs.md) and
[Programs and processes](processes.md).
