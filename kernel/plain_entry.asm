BITS 32

section .text

global _start
global plain_stack_bottom
global plain_stack_top
extern kernel_main

_start:
    cli
    mov esp, plain_stack_top

    push ecx
    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16

plain_stack_bottom:
    resb 16384
plain_stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
