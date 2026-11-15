ENTRY(_start)

SECTIONS
{
    . = 0x00400000;
    __kernel_start = .;

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
        *(.bss.*)

        . = ALIGN(16);
        stack_bottom = .;
        . += 16384;
        stack_top = .;
    }

    __kernel_modules_start = .;
    .kernel_modules : ALIGN(4) {
        KEEP(*(.kernel_modules))
    }
    __kernel_modules_end = .;
    __kernel_end = ALIGN(4K);
}
