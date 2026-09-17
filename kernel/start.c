#include <stdint.h>
#include <write/write.h>
#include <kernel/multiboot.h>
#include <kernel/tar.h>
#include <kernel.h>
#include <driver/keyboard.h>
#include <kernel/time.h>
#include <driver/sound.h>
#include <assert.h>

// Fix Undefined errors
#ifndef NULL
#define NULL ((void *)0)
#endif

extern void info_module_init(void);
extern void initrd_set_base(uint32_t addr); // Bridge for execve

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
    assert(mbi != NULL);

    if (!(mbi->flags & (1 << 3)) || mbi->mods_count == 0) {
        panic("No initramfs provided by GRUB!");
    }

    struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;
    assert(mod != NULL);

    uint32_t initrd_start = mod->mod_start;
    
    // Register base address for process loading via execve
    initrd_set_base(initrd_start);

    k_print("Initramfs loaded successfully!\n");
    
    // Parse the TAR archive in memory
    tar_parse(initrd_start);

    sound_init();
    beep(440, 150);
    sleep_ms(1000);

    k_clear_screen();
    sleep_ms(2000);

    info_module_init();

    k_print("Here we go!\n");

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}