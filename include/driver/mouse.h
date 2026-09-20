#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

struct mouse_state {
    int x;
    int y;
    uint8_t buttons;
};

int init_mouse(uint32_t screen_width, uint32_t screen_height);
int mouse_poll(struct mouse_state *state);

#endif