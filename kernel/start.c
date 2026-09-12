#include <stdint.h>
#include <write/write.h>
#include <kernel.h>

void kernel_main() 
{
    k_clear_screen();
    k_print("Hello, Freestanding Kernel!\n");
    
    // Test the panic function if something goes wrong
    // panic("Test Panic Error");

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}