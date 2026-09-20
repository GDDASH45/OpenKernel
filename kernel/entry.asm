; kernel/entry.asm -- setup grub Multiboot

BITS 32

section .multiboot
align 4

multiboot_header:
    ;dd 0x1BADB002
    ;dd 0x00000007
    ;dd -(0x1BADB002 + 0x00000007)

    dd 0x1BADB002
    dd 0x00000003
    dd -(0x1BADB002 + 0x00000003)

    ;dd 0
    ;dd 1024
    ;dd 768
    ;dd 32

section .text

global _start
extern kernel_main

_start:
    cli

    ; Set up kernel stack
    mov esp, stack_top

    push ebx
    push eax

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .bss

align 16

stack_bottom:
    resb 16384

stack_top:


section .note.GNU-stack noalloc noexec nowrite progbits
