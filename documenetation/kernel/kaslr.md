# Kernel Address Space Layout Randomization (KASLR)

OpenKernel implements 32-bit virtual KASLR while retaining its existing Multiboot entry protocol and fixed physical kernel load address.

## Boot behavior

KASLR is enabled by default. The loader decompresses and validates the linked ELF image, keeps its loadable segments at their normal physical addresses beginning at 0x00400000, chooses a randomized virtual base, applies the ELF relocation records, installs a temporary page directory, and jumps to the relocated virtual entry point.

The randomized kernel virtual window is 0x80000000 through 0xBFFFFFFF. The base is selected in 4 MiB increments, preserving compatibility with the kernel's current 4 MiB page mappings. The user virtual range still begins at 0xC0000000.

The loader keeps identity mappings for bootloader code, Multiboot structures, modules, and other physical addresses. The kernel's virtual alias replaces only the page-directory entries covering the kernel image. The candidate is rejected if that alias would hide Multiboot information, the memory map, module descriptors or payloads, the command line, drive information, bootloader name, VBE structures, or the framebuffer.

The loader seeds a small xorshift generator using the CPU timestamp counter and boot-time values. This provides boot-to-boot variation, but it is not a cryptographically secure random-number generator. Systems requiring strong entropy should add a hardware or firmware entropy source.

If no safe virtual range is available, the loader falls back to the original 0x00400000 link address. The Multiboot magic value, register handoff, and boot information structure remain unchanged.

## ELF relocation requirements

The kernel is linked at 0x00400000. The plain-kernel link uses --emit-relocs to retain static relocation records in the ELF file. The loader applies R_386_32 and R_386_PC32 relocations for allocated sections. R_386_NONE entries are ignored. The build checks the final ELF and fails if it contains another i386 relocation type that the loader cannot process.

Relocation targets are patched at their fixed physical addresses, while symbol values and PC-relative places are adjusted to the randomized virtual layout. Linker-defined absolute symbols inside the kernel image are relocated as well. Symbols outside the image and undefined symbols are left unchanged.

## Paging and memory management

Before jumping to the randomized entry point, the loader enables 4 MiB PSE mappings, keeps a full identity map, and adds virtual-to-physical mappings for the kernel image. This lets the kernel execute at its randomized virtual base while bootloader data remains reachable at its physical addresses.

During physical-memory initialization, the kernel rebuilds its page directory with the same kernel alias. It reserves both the fixed physical kernel image and the physical address range shadowed by the virtual alias, preventing the page allocator from returning pages whose identity mappings have been replaced. VMM CR3 switches translate the kernel page-directory pointer back to its physical address; process page directories remain identity-addressed allocations.

## Boot options

- nokaslr disables randomization.
- kaslr=off disables randomization.
- With neither option, randomization is attempted by default.

The kernel reports its actual runtime virtual base and slide after physical-memory initialization. A zero slide means the image remained at its link address, either because KASLR was disabled or because no safe virtual range was found.

## Scope and limitations

This implementation randomizes the kernel's virtual base. It does not randomize the fixed physical load location, individual kernel sections, the heap, loadable modules, or user address spaces. The loader currently supports R_386_32 and R_386_PC32 relocations for allocated sections. The kernel continues to use 32-bit paging and 4 MiB identity mappings for non-kernel physical memory.
