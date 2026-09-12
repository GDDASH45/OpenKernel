#include <stdint.h>

void sleep_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        // Inner loop tuned for roughly 1ms execution at standard emulation speeds
        for (volatile uint32_t j = 0; j < 40000; j++) {
            __asm__ volatile("nop");
        }
    }
}