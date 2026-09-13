[BITS 32]

global vga_put_cell
global vga_clear_screen_asm

VGA_BUFFER equ 0xB8000

section .text

; void vga_put_cell(uint32_t index, char c, uint8_t color);
vga_put_cell:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]    ; Character index (0 to 1999)
    mov al, [ebp + 12]    ; Character ASCII
    mov ah, [ebp + 16]    ; Color attribute byte

    mov [VGA_BUFFER + edi * 2], ax

    pop edi
    pop ebp
    ret

; void vga_clear_screen_asm(uint8_t color);
vga_clear_screen_asm:
    push ebp
    mov ebp, esp
    push edi

    movzx eax, byte [ebp + 8] ; Color attribute
    shl eax, 8
    or eax, ' '               ; Space character + attribute in ah

    mov edi, VGA_BUFFER
    mov ecx, 2000             ; 80 columns * 25 rows
    rep stosw

    pop edi
    pop ebp
    ret