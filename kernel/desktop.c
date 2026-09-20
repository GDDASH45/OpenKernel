#include <kernel/desktop.h>
#include <driver/mouse.h>
#include <kvideo/fb.h>

#define DESKTOP_BACKGROUND 0x17233B
#define DESKTOP_PANEL 0x243653
#define DESKTOP_PANEL_EDGE 0x3B5175
#define DESKTOP_TEXT 0xE8F0FF
#define DESKTOP_MUTED 0x9DB0CC
#define CURSOR_COLOR 0xFFFFFF
#define CURSOR_SHADOW 0x101828

static void draw_cursor(const struct mouse_state *state) {
    for (int row = 0; row < 12; row++) {
        int width = row / 2 + 1;
        for (int column = 0; column < width; column++) {
            fb_fill_rect(state->x + column, state->y + row, 1, 1,
                         CURSOR_SHADOW);
        }
    }
    for (int row = 0; row < 10; row++) {
        int width = row / 2 + 1;
        for (int column = 0; column < width; column++) {
            fb_fill_rect(state->x + column, state->y + row, 1, 1,
                         CURSOR_COLOR);
        }
    }
}

static void draw_desktop(void) {
    uint32_t width = fb_width();
    uint32_t height = fb_height();

    fb_clear(DESKTOP_BACKGROUND);
    fb_fill_rect(0, 0, (int)width, 42, DESKTOP_PANEL);
    fb_fill_rect(0, 41, (int)width, 1, DESKTOP_PANEL_EDGE);
    fb_fill_rect(24, 92, 360, 220, DESKTOP_PANEL);
    fb_fill_rect(24, 92, 360, 1, DESKTOP_PANEL_EDGE);
    fb_fill_rect(24, 311, 360, 1, DESKTOP_PANEL_EDGE);
    fb_fill_rect(0, (int)height - 34, (int)width, 34, DESKTOP_PANEL);
    fb_draw_text(18, 15, "COOKIEOS", DESKTOP_TEXT);
    fb_draw_text(48, 112, "Desktop", DESKTOP_TEXT);
    fb_draw_text(48, 138, "Framebuffer online", DESKTOP_MUTED);
    fb_draw_text(48, 156, "Mouse ready", DESKTOP_MUTED);
    fb_draw_text(18, (int)height - 24, "COOKIEOS  |  READY", DESKTOP_TEXT);
}

void desktop_run(const struct multiboot_info *mbi) {
    struct mouse_state mouse;
    struct mouse_state previous;

    if (fb_init(mbi) != 0) {
        return;
    }

    draw_desktop();
    mouse.x = (int)fb_width() / 2;
    mouse.y = (int)fb_height() / 2;
    mouse.buttons = 0;
    previous = mouse;
    draw_cursor(&mouse);

    for (;;) {
        if (mouse_poll(&mouse) == 1 &&
            (mouse.x != previous.x || mouse.y != previous.y ||
             mouse.buttons != previous.buttons)) {
            draw_desktop();
            draw_cursor(&mouse);
            previous = mouse;
        }
        __asm__ volatile ("nop");
    }
}

void desktop_show(const struct multiboot_info *mbi) {
    struct mouse_state mouse;

    if (fb_init(mbi) != 0) {
        return;
    }

    draw_desktop();
    mouse.x = (int)fb_width() / 2;
    mouse.y = (int)fb_height() / 2;
    mouse.buttons = 0;
    draw_cursor(&mouse);
}