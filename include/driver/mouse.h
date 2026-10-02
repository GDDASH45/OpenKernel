#ifndef OPENKERNEL_MOUSE_H
#define OPENKERNEL_MOUSE_H

#include <stdint.h>

struct mouse_state {
    int32_t x;
    int32_t y;
    uint8_t buttons;
};

int init_mouse(uint32_t screen_width, uint32_t screen_height);
int mouse_is_ready(void);
int mouse_poll(struct mouse_state *state);
void mouse_irq_handler(void);

#endif