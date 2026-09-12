#include <stdint.h>
#include <write/write.h>
#include <kernel/multiboot.h>
#include <kernel/tar.h>
#include <kernel.h>
#include <driver/keyboard.h>
#include <kernel/time.h>
#include <driver/sound.h>

extern void info_module_init(void);

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

    k_print("Initramfs loaded successfully!\n");
    
    // Parse the TAR archive in memory
    tar_parse(initrd_start);

    sound_init();

    beep(440, 150);
    beep(430, 150);
    beep(420, 150);
    beep(410, 150);
    beep(400, 150);
    beep(410, 150);
    beep(420, 150);
    beep(430, 150);
    beep(440, 150);

    //info_module_init();

    sleep_ms(1000);

    k_clear_screen();

    sleep_ms(2000);

    for (;;)
    {
        // If interrupts aren't enabled yet, use a simple busy loop
        // instead of hlt to prevent a permanent freeze:
        __asm__ volatile ("nop");
    }
}