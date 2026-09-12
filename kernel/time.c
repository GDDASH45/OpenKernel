#include <stdint.h>

void sleep_ms(uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 40000; j++) {
            __asm__ __volatile__("nop");
        }
    }
}