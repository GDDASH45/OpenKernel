# Build and run

## Requirements

The normal kernel build targets 32-bit x86 and uses GCC-compatible compiler
support for `-m32`, GNU binutils, and NASM. Building an ISO and running it also
requires GRUB tooling, ISO creation utilities, and QEMU. Package names differ
by distribution; on Debian/Ubuntu, the common packages include `build-essential`,
`nasm`, `qemu-system-x86`, `grub-pc-bin`, `xorriso`, and `mtools`.

## Build the kernel

From the repository root:

```sh
make -j"$(nproc)"
```

The default output is `build/kernel.bin`. `make linux` additionally builds
`build/openkernel`, a small Linux launcher that starts QEMU; it is not a
hosted implementation of OpenKernel's kernel or shell.

## Create and boot the ISO

`make run-linux` builds the kernel and launcher, creates the initramfs from
`initfs/`, copies the kernel into the ISO tree, creates `os.iso`, and launches
QEMU. The equivalent scripts can be run separately: `build_fs.sh`,
`copy_kernel.sh`, then `make_iso.sh`. GRUB configuration is under
`iso/boot/grub/`.

Pass QEMU arguments through Make, for example:

```sh
make run-linux QEMU_ARGS="-m 256"
```

The launcher can be selected with `OPENKERNEL_QEMU`; its ISO path can be set
with `OPENKERNEL_ISO`. `run.sh` is a separate convenience script and currently
invokes QEMU directly with its own audio options.

## Hosted launcher versus kernel

`host/main.c` only calls `execvp()` to start QEMU with the ISO. It does not
emulate system calls, provide a Linux-hosted VFS, or run the shell natively.
See [Architecture and current status](../kernel/overview.md) for the security
and process-model limitations of the kernel itself.
