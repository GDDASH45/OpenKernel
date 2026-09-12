#include <stdint.h>
#include <write/write.h>
#include <kernel/multiboot.h>
#include <kernel.h>

void kernel_main(uint32_t magic, uint32_t multiboot_addr) 
{
    k_clear_screen();
    if (magic != 0x2BADB002) {
        panic("Invalid multiboot magic number!");
    }

    struct multiboot_info *mbi = (struct multiboot_info *)multiboot_addr;

    if (!(mbi->flags & (1 << 3)) || mbi->mods_count == 0) {
        panic("No initramfs provided by GRUB!");
    }

    struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;

    uint32_t initrd_start = mod->mod_start;
    uint32_t initrd_end = mod->mod_end;


    k_print("Initramfs loaded successfully!\n");
    

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}