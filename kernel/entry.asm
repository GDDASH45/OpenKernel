; kernel/entry.asm -- setup GRUB Multiboot

BITS 32

section .multiboot
align 4

multiboot_header:
    dd 0x1BADB002
    dd 0x00000007
    dd -(0x1BADB002 + 0x00000007)
    dd 0
    dd 1024
    dd 768
    dd 32

section .text

global _start
global kernel_loader_jump
global stack_bottom
global stack_top
extern kernel_loader_main

_start:
    cli

    ; Set up kernel stack
    mov esp, stack_top

    ; OpenKernel boot information
    ;
    ; ECX = OEM flag
    ; 1 = OEM boot
    ; 0 = normal boot
    ;
    mov ecx, 0

    ; kernel_loader_main(eax, ebx, ecx)
    push ecx
    push ebx
    push eax

    call kernel_loader_main

.hang:
    cli
    hlt
    jmp .hang

kernel_loader_jump:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    mov ebx, [esp + 12]
    mov ecx, [esp + 16]
    jmp edx

section .bss

align 16

stack_bottom:
    resb 16384

stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits