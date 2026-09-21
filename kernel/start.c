#include <stdint.h>
#include <write/write.h>
#include <kernel/multiboot.h>
#include <kernel/tar.h>
#include <kernel.h>
#include <driver/keyboard.h>
#include <driver/mouse.h>
#include <kernel/time.h>
#include <driver/sound.h>
#include <assert.h>
#include <init/path.h>
#include <kernel/desktop.h>
#include <kernel/okfs.h>

// Fix Undefined errors
#ifndef NULL
#define NULL ((void *)0)
#endif

extern void info_module_init(void);
extern void initrd_set_base(uint32_t addr); // Bridge for execve
extern void draw_bmp(struct multiboot_info *mbi);
extern void fade_bmp_to_dust(struct multiboot_info *mbi);

static uint32_t string_length(const char *text) {
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void create_system_files(void) {
    static const char *paths[] = {
        "/etc/hostname",
        "/etc/motd",
        "/etc/version",
        "/home/readme.txt",
        "/home/welcome.txt",
        "/var/log/boot.log",
        "/var/log/kernel.log",
        "/usr/share/license.txt"
    };
    static const char *contents[] = {
        "openkernel\n",
        "Welcome to OpenKernel.\n",
        "OpenKernel persistent filesystem\n",
        "This file was created by the kernel.\n",
        "Your virtual disk is writable.\n",
        "OpenKernel boot completed.\n",
        "ATA and OKFS are online.\n",
        "OpenKernel\n"
    };

    for (uint32_t index = 0; index < sizeof(paths) / sizeof(paths[0]); index++) {
        okfs_write(paths[index], contents[index], string_length(contents[index]));
    }
}

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
    draw_bmp(mbi);
    fade_bmp_to_dust(mbi);

    if (init_mouse(mbi->framebuffer_width, mbi->framebuffer_height) != 0) {
        panic("PS/2 mouse initialization failed!");
    }

    desktop_show(mbi);

    struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;
    assert(mod != NULL);

    uint32_t initrd_start = mod->mod_start;

    if (okfs_mount() != 0) {
        k_print("[FS] Persistent storage unavailable.\n");
    } else {
        if (okfs_unpack_tar(initrd_start) < 0) {
            k_print("[FS] Initramfs unpack failed.\n");
        } else {
            k_print("[FS] Initramfs unpacked to persistent storage.\n");
            create_system_files();
        }
    }
    
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

    k_clear_screen();

    k_print("Here we go!\n");

    init_path();
    desktop_run(mbi);
}