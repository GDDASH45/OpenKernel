[BITS 32]

global fb_put_pixel_asm
global fb_clear_asm

section .text

; void fb_put_pixel_asm(uint32_t *fb_addr, uint32_t width, uint32_t x, uint32_t y, uint32_t color);
fb_put_pixel_asm:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]      ; Framebuffer base address
    mov ecx, [ebp + 12]     ; Screen width in pixels
    mov eax, [ebp + 20]     ; Y coordinate
    mul ecx                 ; eax = y * width
    add eax, [ebp + 16]     ; eax = (y * width) + x
    shl eax, 2              ; Multiply by 4 bytes per pixel (32-bit RGBA)
    add edi, eax            ; Target pixel memory address

    mov edx, [ebp + 24]     ; Color value
    mov [edi], edx          ; Write color to framebuffer

    pop edi
    pop ebp
    ret

; void fb_clear_asm(uint32_t *fb_addr, uint32_t total_pixels, uint32_t color);
fb_clear_asm:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]      ; Framebuffer base address
    mov ecx, [ebp + 12]     ; Total number of pixels (width * height)
    mov eax, [ebp + 16]     ; Color value
    
    rep stosd               ; Fast 32-bit block fill

    pop edi
    pop ebp
    ret