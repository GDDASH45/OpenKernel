ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .multiboot : {
        *(.multiboot)
    }

    .text BLOCK(4K) : ALIGN(4K) {
        *(.text)
        *(.init.text)
        *(.exit.text)
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