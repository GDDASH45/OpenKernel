# Real-hardware boot readiness

OpenKernel currently boots through a GRUB Multiboot 1 handoff. The first
real-machine target is a legacy BIOS x86 PC that can boot a GRUB USB drive or
CD/DVD. This is a staged target, not a claim of compatibility with every PC.

## Current platform requirements

- 32-bit x86 protected mode and a GRUB Multiboot 1 loader.
- A CPU with CPUID and the Page Size Extension (PSE) feature. OpenKernel uses
  4 MiB page-directory entries during early paging setup.
- A usable Multiboot memory map.
- A VGA-compatible text display for the loader's early fatal-error message.
- Legacy PCI configuration mechanism #1 for PCI enumeration.
- A PS/2-compatible keyboard controller for the current required keyboard
  initialization path.

The loader checks CPUID and PSE before loading the kernel. It stops with a
specific message rather than attempting to enable unsupported 4 MiB paging.

## Hardware discovery

kernel/pci.c scans PCI buses, devices, and multifunction devices using
configuration mechanism #1 (I/O ports 0xCF8 and 0xCFC). It reports each
discovered function's bus/device/function address, vendor/device IDs, class,
subclass, and programming interface.

Enumeration is discovery only. It does not probe or size BARs, enable bus
mastering, assign resources, or bind a driver. Those operations need to be
implemented by the relevant device-driver and resource-management layers.

ACPI support currently discovers the firmware tables needed for the reset
register. It is not yet a complete ACPI platform implementation.

## Known gaps before broad hardware support

- No UEFI-native loader or GOP framebuffer handoff.
- No Local APIC/IOAPIC interrupt-controller support; the current interrupt
  path uses the legacy PIC.
- No SMP startup or per-CPU initialization.
- No general PCI resource allocation or driver binding.
- No AHCI or NVMe storage driver; storage support is limited to existing
  legacy ATA paths.
- No general USB host-controller or USB input stack.
- No guarantee that firmware exposes a usable PS/2 controller.
- No broad chipset, power-management, or laptop compatibility testing.

## Safe bring-up procedure

1. Start with a spare machine whose firmware can boot legacy BIOS/CSM media.
2. Keep the machine's existing operating system and disks disconnected or
   backed up. Do not use the kernel's disk-formatting or installation tools
   against a system disk.
3. Boot from removable media and capture the complete serial or photographed
   boot log. A serial debug console is not yet guaranteed on every machine.
4. Verify memory totals and the PCI inventory before attempting storage writes.
5. Test one device class at a time. Do not assume a PCI function is supported
   merely because it appears in the inventory.
6. Keep QEMU as the regression platform while adding real-machine support.

The supported boot path and hardware limitations are documented here so that
future platform work can be added incrementally without treating a successful
QEMU boot as proof of real-hardware compatibility.
