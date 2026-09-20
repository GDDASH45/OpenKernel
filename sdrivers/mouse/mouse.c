#include <driver/mouse.h>
#include <kernel/ports.h>

#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define PS2_DATA 0x60
#define PS2_OUTPUT_FULL 0x01
#define PS2_INPUT_FULL 0x02
#define PS2_AUX_DATA 0x20

static int mouse_x;
static int mouse_y;
static int mouse_max_x;
static int mouse_max_y;
static uint8_t packet[3];
static uint8_t packet_index;

static int wait_for_write(void) {
    uint32_t timeout = 100000;
    while ((port_byte_in(PS2_STATUS) & PS2_INPUT_FULL) && timeout != 0) {
        timeout--;
    }
    return timeout != 0;
}

static int wait_for_mouse_data(void) {
    uint32_t timeout = 100000;
    while (timeout != 0) {
        uint8_t status = port_byte_in(PS2_STATUS);
        if ((status & (PS2_OUTPUT_FULL | PS2_AUX_DATA)) ==
            (PS2_OUTPUT_FULL | PS2_AUX_DATA)) {
            return 1;
        }
        timeout--;
    }
    return 0;
}

static int send_mouse_command(uint8_t command) {
    if (!wait_for_write()) {
        return 0;
    }
    port_byte_out(PS2_COMMAND, 0xD4);
    if (!wait_for_write()) {
        return 0;
    }
    port_byte_out(PS2_DATA, command);
    if (!wait_for_mouse_data()) {
        return 0;
    }
    return port_byte_in(PS2_DATA) == 0xFA;
}

int init_mouse(uint32_t screen_width, uint32_t screen_height) {
    uint8_t config;

    if (screen_width == 0 || screen_height == 0 || !wait_for_write()) {
        return -1;
    }

    port_byte_out(PS2_COMMAND, 0xA8);
    if (!wait_for_write()) {
        return -1;
    }
    port_byte_out(PS2_COMMAND, 0x20);
    if (!wait_for_mouse_data()) {
        return -1;
    }
    config = port_byte_in(PS2_DATA);
    config |= 0x02;
    config &= (uint8_t)~0x20;

    if (!wait_for_write()) {
        return -1;
    }
    port_byte_out(PS2_COMMAND, 0x60);
    if (!wait_for_write()) {
        return -1;
    }
    port_byte_out(PS2_DATA, config);

    if (!send_mouse_command(0xF6) || !send_mouse_command(0xF4)) {
        return -1;
    }

    mouse_max_x = (int)screen_width - 1;
    mouse_max_y = (int)screen_height - 1;
    mouse_x = mouse_max_x / 2;
    mouse_y = mouse_max_y / 2;
    packet_index = 0;
    return 0;
}

int mouse_poll(struct mouse_state *state) {
    uint8_t status;
    int delta_x;
    int delta_y;

    if (state == 0) {
        return -1;
    }

    status = port_byte_in(PS2_STATUS);
    if ((status & (PS2_OUTPUT_FULL | PS2_AUX_DATA)) !=
        (PS2_OUTPUT_FULL | PS2_AUX_DATA)) {
        return 0;
    }

    packet[packet_index++] = port_byte_in(PS2_DATA);
    if (packet_index == 1 && !(packet[0] & 0x08)) {
        packet_index = 0;
        return 0;
    }
    if (packet_index < 3) {
        return 0;
    }

    packet_index = 0;
    delta_x = (packet[0] & 0x10) ? (int)packet[1] - 256 : packet[1];
    delta_y = (packet[0] & 0x20) ? (int)packet[2] - 256 : packet[2];
    mouse_x += delta_x;
    mouse_y -= delta_y;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > mouse_max_x) mouse_x = mouse_max_x;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > mouse_max_y) mouse_y = mouse_max_y;

    state->x = mouse_x;
    state->y = mouse_y;
    state->buttons = packet[0] & 0x07;
    return 1;
}