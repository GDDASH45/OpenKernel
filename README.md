# OpenKernel

OpenKernel is a custom, minimalist microkernel designed and implemented in **C**. It features a clean separation between the kernel core and userspace, a custom `PMX` binary loader, and `tar` initramfs parsing.

## Architecture Overview

* **Microkernel Core**: Manages primitive CPU exceptions, memory quotas, and execution handoff.
* **Userspace Separation**: Core services and utilities execute entirely in userspace, keeping the kernel decoupled from application logic.
* **PMX Binary Loader**: Custom binary format loader responsible for mapping and executing programs.
* **Initramfs VFS**: Parses standard `tar` archives to mount an initial ramdisk filesystem during boot.

## Contribution Policy

OpenKernel is written strictly in **C**. 

> **Important**: All contributions, kernel subsystems, and userspace programs must be written in standard C. Code submitted in any other programming language will not be accepted.

## Building and Running

You must run on a linux environment. Run this command if on Ubuntu

sudo apt update && sudo apt install build-essential nasm qemu-system-x86 genisoimage xorriso

You can also find a minimal initramfs.img in bin/initramfs.img

For now as of 0.0.1-rc1 it can only understand a ustar tar archive
The initramfs must have a init script

## OKX shared objects

`okcc.sh source.c output.oso` packages a flat `OSO1` shared object. The source
must define an `int32_t _oso_entry(void)` function. At runtime, userspace can
call `oso_load("/path/to/object.oso")` from `<user/process.h>`; it returns an
`oso_entry_t` function pointer or null if the object could not be found or
loaded. The returned function can then be called directly.

This initial OSO format is a fixed-address flat image (loaded at `0x300000`),
not an ELF-style relocatable object: objects must be self-contained, and only
the entry function is exported. It shares the current process address space
and does not yet support relocation, unloading, or multiple objects at
different load addresses. Keep each OSO within its reserved 1 MiB region.

run these scripts in order:

make -j$(nproc)

make a iso/boot/grub.cfg that is something like this:

set timeout=0
set default=0

clear

menuentry "Open Kernel" {
    multiboot /boot/kernel.bin
    module /boot/initramfs.img
    boot
}

./copy_kernel.sh

./make_iso.sh

this should produce os.iso


Ensure you have a cross-compiler toolchain and QEMU installed, then run the build and boot script:

```bash
./run.sh
```

## Linux-hosted console

To build a native Linux executable that runs the hosted console and its in-memory
VFS (without booting the kernel or accessing hardware), run:

```sh
make linux
./build/openkernel
```

Alternatively, `make run-linux` builds and launches it. This host mode is a
development console, not a full hardware or kernel emulator; the normal
freestanding kernel build remains `make`. At the `ok>` prompt, use `run
/path/to/program [arguments...]` to start a Linux executable (typically an ELF)
as a child process; for example, `run /bin/echo hello`. Such programs run with
the current user's Linux permissions and are not sandboxed by OpenKernel.