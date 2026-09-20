#include <kvideo/fb.h>
#include <kvideo/font8x10.h>

#define VGA_GFX_MEM 0xA0000
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

static uint32_t cursor_offset = 0;
static volatile uint8_t *framebuffer;
static uint32_t framebuffer_pitch;
static uint32_t framebuffer_width;
static uint32_t framebuffer_height;
static uint8_t framebuffer_bpp;
static uint8_t red_position;
static uint8_t red_mask;
static uint8_t green_position;
static uint8_t green_mask;
static uint8_t blue_position;
static uint8_t blue_mask;

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

int fb_init(const struct multiboot_info *mbi) {
    cursor_offset = 0;
    if (mbi == 0 || !(mbi->flags & (1 << 12)) ||
        mbi->framebuffer_type != 1 || mbi->framebuffer_bpp < 15 ||
        mbi->framebuffer_bpp > 32 || mbi->framebuffer_addr == 0 ||
        mbi->framebuffer_width == 0 || mbi->framebuffer_height == 0) {
        return -1;
    }

    framebuffer = (volatile uint8_t *)(uintptr_t)mbi->framebuffer_addr;
    framebuffer_pitch = mbi->framebuffer_pitch;
    framebuffer_width = mbi->framebuffer_width;
    framebuffer_height = mbi->framebuffer_height;
    framebuffer_bpp = mbi->framebuffer_bpp;
    red_position = mbi->framebuffer_red_field_position;
    red_mask = mbi->framebuffer_red_mask_size;
    green_position = mbi->framebuffer_green_field_position;
    green_mask = mbi->framebuffer_green_mask_size;
    blue_position = mbi->framebuffer_blue_field_position;
    blue_mask = mbi->framebuffer_blue_mask_size;

    if (red_mask == 0 && green_mask == 0 && blue_mask == 0 &&
        framebuffer_bpp >= 24) {
        red_position = 16;
        red_mask = 8;
        green_position = 8;
        green_mask = 8;
        blue_position = 0;
        blue_mask = 8;
    }
    return 0;
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

static uint32_t scale_channel(uint8_t value, uint8_t bits) {
    uint32_t maximum = (1u << bits) - 1u;
    return ((uint32_t)value * maximum + 127u) / 255u;
}

static uint32_t pack_color(uint32_t color) {
    uint32_t red = scale_channel((uint8_t)(color >> 16), red_mask);
    uint32_t green = scale_channel((uint8_t)(color >> 8), green_mask);
    uint32_t blue = scale_channel((uint8_t)color, blue_mask);
    return (red << red_position) | (green << green_position) |
           (blue << blue_position);
}

static void put_rgb_pixel(int x, int y, uint32_t color) {
    volatile uint8_t *pixel;
    uint32_t packed_color;

    if (framebuffer == 0 || x < 0 || y < 0 || x >= (int)framebuffer_width ||
        y >= (int)framebuffer_height) {
        return;
    }
    pixel = framebuffer + y * framebuffer_pitch + x * (framebuffer_bpp / 8);
    packed_color = pack_color(color);
    for (uint32_t byte = 0; byte < framebuffer_bpp / 8; byte++) {
        pixel[byte] = (uint8_t)(packed_color >> (byte * 8));
    }
}

void fb_clear(uint32_t color) {
    fb_fill_rect(0, 0, (int)framebuffer_width, (int)framebuffer_height, color);
}

void fb_fill_rect(int x, int y, int width, int height, uint32_t color) {
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            put_rgb_pixel(x + column, y + row, color);
        }
    }
}

void fb_draw_char(int x, int y, char character, uint32_t color) {
    const char *glyph = font8x10_get((unsigned char)character);

    for (int row = 0; row < FONT8X10_HEIGHT; row++) {
        for (int column = 0; column < FONT8X10_WIDTH; column++) {
            if ((glyph[row] & (char)(0x80 >> column)) != 0) {
                put_rgb_pixel(x + column, y + row, color);
            }
        }
    }
}

void fb_draw_text(int x, int y, const char *text, uint32_t color) {
    while (*text != '\0') {
        if (*text == '\n') {
            x = 0;
            y += FONT8X10_HEIGHT;
        } else {
            fb_draw_char(x, y, *text, color);
            x += FONT8X10_WIDTH;
        }
        text++;
    }
}

uint32_t fb_width(void) {
    return framebuffer_width;
}

uint32_t fb_height(void) {
    return framebuffer_height;
}