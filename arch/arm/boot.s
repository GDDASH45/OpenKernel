.global _start
.extern kernel_main

.section .multiboot
.align 4
multiboot_header:
    .long 0x1BADB002                  ; Multiboot magic number
    .long 0x00000003                  ; Flags (page align + memory info)
    .long -(0x1BADB002 + 0x00000003)  ; Checksum

.section .text
_start:
    ; GRUB Multiboot on ARM passes:
    ; r0 = magic number (0x2BADB002)
    ; r1 = physical address of multiboot_info structure
    
    ldr sp, =_stack_top

    ; Clear BSS section
    ldr r2, =__bss_start
    ldr r3, =__bss_end
    mov r4, #0

.clear_bss:
    cmp r2, r3
    bge .call_kernel
    strb r4, [r2], #1
    b .clear_bss

.call_kernel:
    ; Matches kernel_main(uint32_t magic, uint32_t multiboot_addr)
    bl kernel_main

.hang:
    wfi
    b .hang