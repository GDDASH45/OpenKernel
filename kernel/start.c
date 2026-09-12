// kernel/start.c -- main entry point

#include <stdint.h>

void kernel_main() 
{
    volatile unsigned short* vga_buffer = (volatile unsigned short*)0xB8000;
    
    // "Hello" string with light cyan text on a black background (Attribute: 0x0F / 0x0B)
    const char* str = "Hello";
    unsigned char color = 0x0B; // Light cyan
    
    for (int i = 0; str[i] != '\0'; i++) {
        vga_buffer[i] = (unsigned short)str[i] | ((unsigned short)color << 8);
    }

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}