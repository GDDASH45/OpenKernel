#include <stdint.h>
#include <write/write.h>
#include <kernel/multiboot.h>
#include <kernel/tar.h>
#include <kernel.h>
#include <drivers/keyboard.h>

void kernel_main(uint32_t magic, uint32_t multiboot_addr) 
{
    k_clear_screen();
    if (magic != 0x2BADB002) {
        panic("Invalid multiboot magic number!");
    }

    if (init_keyboard() != 0) {
        panic("Keyboard initialization failed!");
    }

    struct multiboot_info *mbi = (struct multiboot_info *)multiboot_addr;

    if (!(mbi->flags & (1 << 3)) || mbi->mods_count == 0) {
        panic("No initramfs provided by GRUB!");
    }

    struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;

    uint32_t initrd_start = mod->mod_start;
    uint32_t initrd_end = mod->mod_end;

    k_print("Initramfs loaded successfully!\n");
    
    // Parse the TAR archive in memory
    tar_parse(initrd_start);

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}