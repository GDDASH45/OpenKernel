BITS 32

section .text

global process_context_save
global process_context_restore

; int process_context_save(uint32_t *context)
; context = EBX, ESI, EDI, EBP, resume ESP, resume EIP
process_context_save:
    mov eax, [esp + 4]
    mov [eax + 0], ebx
    mov [eax + 4], esi
    mov [eax + 8], edi
    mov [eax + 12], ebp
    lea edx, [esp + 4]
    mov [eax + 16], edx
    mov edx, [esp]
    mov [eax + 20], edx
    xor eax, eax
    ret

; noreturn void process_context_restore(uint32_t *context, int32_t status)
process_context_restore:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    mov ecx, [edx + 20]
    mov edx, [edx + 16]
    mov ebx, [esp + 4]
    mov ebx, [ebx + 0]
    mov esi, [esp + 4]
    mov esi, [esi + 4]
    mov edi, [esp + 4]
    mov edi, [edi + 8]
    mov ebp, [esp + 4]
    mov ebp, [ebp + 12]
    mov esp, edx
    jmp ecx

section .note.GNU-stack noalloc noexec nowrite progbits
