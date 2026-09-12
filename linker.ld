ENTRY(_start)

SECTIONS
{
    /* Typically loaded at 1MB for traditional x86 kernels */
    . = 0x100000;

    .text BLOCK(4K) : ALIGN(4K) {
        *(.multiboot)
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata*)
    }

    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }
}