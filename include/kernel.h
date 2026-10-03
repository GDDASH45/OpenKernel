#ifndef KERNEL_H
#define KERNEL_H

#include <write/write.h>
#include <driver/sound.h>
#include <kvideo/fb.h>

static inline void panic(const char* message) {
    k_set_quiet(0);
    beep(990, 150); // Why not beep when the kernel panics?
    fb_panic_screen(message);
    k_print("KERNEL PANIC!!\n");
    k_print(message);
    k_print("\n");
    k_print("END KERNEL PANIC -- ");
    k_print(message);

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}

#endif