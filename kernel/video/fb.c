#include <kvideo/fb.h>

#define VGA_GFX_MEM 0xA0000
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

static uint32_t cursor_offset = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void set_mode_13h(void) {
    // Standard VGA Mode 13h sequence using CRT controller and misc registers
    outb(0x3C2, 0x63);
    
    // Sequencer registers
    const uint8_t seq_regs[] = {0x03, 0x01, 0x0F, 0x00, 0x0E};
    for (int i = 0; i < 5; i++) {
        outb(0x3C4, i);
        outb(0x3C5, seq_regs[i]);
    }

    // CRTC registers
    outb(0x3D4, 0x11); outb(0x3D5, 0x0E); // Unlock CRTC
    const uint8_t crtc_regs[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
        0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF
    };
    for (int i = 0; i < 25; i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc_regs[i]);
    }

    // Graphics Controller registers
    const uint8_t gc_regs[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF};
    for (int i = 0; i < 9; i++) {
        outb(0x3CE, i);
        outb(0x3CF, gc_regs[i]);
    }

    // Attribute Controller registers
    const uint8_t ac_regs[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x41, 0x00, 0x0F, 0x00, 0x00
    };
    for (int i = 0; i < 21; i++) {
        __asm__ volatile ("inb $0x3DA, %%al" : :); // Reset flip-flop
        outb(0x3C0, i);
        outb(0x3C0, ac_regs[i]);
    }
    outb(0x3C0, 0x20); // Enable video palette output
}

void fb_init(void) {
    cursor_offset = 0;
}

int fb_write(const char *buf, uint32_t size) {
    volatile uint8_t *screen = (volatile uint8_t *)VGA_GFX_MEM;
    for (uint32_t i = 0; i < size; i++) {
        if (cursor_offset < (SCREEN_WIDTH * SCREEN_HEIGHT)) {
            screen[cursor_offset++] = (uint8_t)buf[i];
        }
    }
    return (int)size;
}

void fb_put_pixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        volatile uint8_t *screen = (volatile uint8_t *)VGA_GFX_MEM;
        screen[(y * SCREEN_WIDTH) + x] = color;
    }
}

void fb_set_pixel(int x, int y, uint8_t color) {
    fb_put_pixel(x, y, color);
}