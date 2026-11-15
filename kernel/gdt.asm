BITS 32

section .text

global gdt_load
global gdt_enter_user_mode

; void gdt_load(const struct gdt_pointer *pointer)
gdt_load:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.reload_cs
.reload_cs:
    mov ax, 0x28
    ltr ax
    ret

; noreturn void gdt_enter_user_mode(uint32_t entry, uint32_t user_stack)
gdt_enter_user_mode:
    mov ecx, [esp + 4]
    mov edx, [esp + 8]

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push dword 0x23
    push edx
    pushfd
    pop eax
    or eax, 0x200
    push eax
    push dword 0x1B
    push ecx
    iretd

section .note.GNU-stack noalloc noexec nowrite progbits
