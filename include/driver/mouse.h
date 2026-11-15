#ifndef OPENKERNEL_MOUSE_H
#define OPENKERNEL_MOUSE_H

#include <stdint.h>

#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04
#define MOUSE_BUTTON_4      0x08
#define MOUSE_BUTTON_5      0x10

struct mouse_state {
    int32_t x;
    int32_t y;
    uint8_t buttons;
    uint8_t pressed;
    uint8_t released;
    int32_t delta_x;
    int32_t delta_y;
    int32_t wheel;
    uint32_t packets;
};

int init_mouse(uint32_t screen_width, uint32_t screen_height);
int mouse_is_ready(void);
void mouse_set_bounds(uint32_t screen_width, uint32_t screen_height);
void mouse_set_position(int32_t x, int32_t y);
int mouse_poll(struct mouse_state *state);
void mouse_irq_handler(void);

#endif
