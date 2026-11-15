# Kernel Address Space Layout Randomization (KASLR)

OpenKernel's 32-bit loader supports physical kernel-image randomization without changing the Multiboot handoff or the kernel's identity-mapped address model.

## Boot behavior

KASLR is enabled by default. The loader decompresses the linked ELF image into its private buffer, validates its loadable segments, selects a safe physical destination, copies each segment to the selected address, applies the retained ELF relocations, and transfers control to the relocated entry point.

The selected slide is a multiple of 4 MiB. This preserves compatibility with the kernel's current 4 MiB identity page mappings. Candidate image ranges are restricted to 16 MiB through 2 GiB and must fit inside a usable Multiboot memory-map region. The loader rejects candidates that overlap the Multiboot information structure, memory map, module descriptor table, loaded modules, or command line.

The loader seeds a small xorshift generator using the CPU timestamp counter and boot-time values. This provides boot-to-boot variation, but it is not a cryptographically secure random-number generator. Systems requiring strong entropy should add a hardware or firmware entropy source.

If no safe candidate is available, the loader uses the original link address. The boot protocol and the Multiboot magic/register handoff remain unchanged.

## ELF relocation requirements

The kernel is linked at 0x00400000. The plain-kernel link now uses --emit-relocs, retaining static relocation records in the ELF file. The loader currently applies R_386_32 and R_386_PC32 relocations for allocated sections. R_386_NONE entries are ignored. An unsupported relocation causes the loader to reject the image instead of jumping into a partially relocated kernel.

The relocation pass adjusts both relocation targets and symbol addresses that belong to the linked kernel image. Symbols outside the image and absolute symbols are left unchanged.

## Boot options

- nokaslr disables randomization.
- kaslr=off disables randomization.
- With neither option, randomization is attempted by default.

The kernel reports its actual runtime base and slide after physical-memory initialization. A zero slide means the image remained at its link address, either because KASLR was disabled or because no safe destination was found.

## Scope and limitations

This implementation randomizes the physical placement of the complete kernel image. It does not randomize individual kernel sections, the heap, modules, user address spaces, or the kernel's virtual base. The kernel continues to use identity mapping and the existing 32-bit page-directory setup.

The loader uses the Multiboot memory map when available and falls back to the legacy mem_upper field otherwise. The selected image must fit in one usable RAM region. The loader's decompression buffer remains at its original address and is never moved.
