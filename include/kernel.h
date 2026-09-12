#ifndef KERNEL_H
#define KERNEL_H

#include <write/write.h>

static inline void panic(const char* message) {
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