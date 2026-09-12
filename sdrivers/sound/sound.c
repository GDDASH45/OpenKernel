#include <driver/sound.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

extern void sleep_ms(uint32_t ms);

void sound_init(void) {
    nosound();
}

void play_sound(uint32_t frequency) {
    if (frequency == 0) {
        nosound();
        return;
    }

    uint32_t div = 1193180 / frequency;
    outb(0x43, 0xB6); // PIT channel 2 configuration
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    // Enable PC speaker output via port 0x61
    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3) {
        outb(0x61, tmp | 3);
    }
}

void nosound(void) {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

void beep(uint32_t frequency, uint32_t duration_ms) {
    play_sound(frequency);
    sleep_ms(duration_ms);
    nosound();
}