#include <driver/mouse.h>
#include <kernel/interrupts.h>
#include <kernel/ports.h>

#define PS2_STATUS       0x64
#define PS2_COMMAND      0x64
#define PS2_DATA         0x60

#define PS2_OUTPUT_FULL  0x01
#define PS2_INPUT_FULL   0x02
#define PS2_AUX_DATA     0x20

#define MOUSE_ACK        0xFA

static volatile int mouse_x;
static volatile int mouse_y;
static int mouse_max_x;
static int mouse_max_y;
static volatile uint8_t mouse_buttons;
static volatile uint32_t mouse_generation;
static uint32_t mouse_seen_generation;

static uint8_t packet[3];
static uint8_t packet_index;
static int mouse_ready;

static int wait_for_write(void) {
    uint32_t timeout = 10000;

    while ((port_byte_in(PS2_STATUS) & PS2_INPUT_FULL) && timeout != 0) {
        timeout--;
    }
    return timeout != 0;
}

static int wait_for_read(void) {
    uint32_t timeout = 10000;

    while (!(port_byte_in(PS2_STATUS) & PS2_OUTPUT_FULL) && timeout != 0) {
        timeout--;
    }
    return timeout != 0;
}

static int wait_for_mouse_data(void) {
    uint32_t timeout = 10000;

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
    return port_byte_in(PS2_DATA) == MOUSE_ACK;
}

int init_mouse(uint32_t screen_width, uint32_t screen_height) {
    uint8_t config;

    if (screen_width == 0 || screen_height == 0 ||
        screen_width > 0x7FFFFFFFu || screen_height > 0x7FFFFFFFu) {
        return -1;
    }
    if (mouse_ready) {
        mouse_max_x = (int)screen_width - 1;
        mouse_max_y = (int)screen_height - 1;
        return 0;
    }

    if (!wait_for_write()) {
        return -1;
    }
    port_byte_out(PS2_COMMAND, 0xA8);

    if (!wait_for_write()) {
        return -1;
    }
    port_byte_out(PS2_COMMAND, 0x20);
    if (!wait_for_read()) {
        return -1;
    }
    config = port_byte_in(PS2_DATA);

    /* Enable auxiliary-device IRQs and the mouse clock. */
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
    mouse_buttons = 0;
    packet_index = 0;
    mouse_generation = 0;
    mouse_seen_generation = 0;
    mouse_ready = 1;

    /* IRQ12 stays masked until reporting and packet state are ready. */
    interrupts_enable_irq(12);
    return 0;
}

int mouse_is_ready(void) {
    return mouse_ready;
}

void mouse_irq_handler(void) {
    uint8_t status = port_byte_in(PS2_STATUS);
    uint8_t data;
    int delta_x;
    int delta_y;

    if (!mouse_ready || (status & (PS2_OUTPUT_FULL | PS2_AUX_DATA)) !=
        (PS2_OUTPUT_FULL | PS2_AUX_DATA)) {
        return;
    }

    data = port_byte_in(PS2_DATA);
    if (packet_index == 0) {
        if ((data & 0x08) == 0) {
            return;
        }
        packet[0] = data;
        packet_index = 1;
        return;
    }

    packet[packet_index++] = data;
    if (packet_index < 3) {
        return;
    }
    packet_index = 0;

    /* Ignore packets whose motion overflowed the signed byte range. */
    if (packet[0] & 0xC0) {
        return;
    }

    delta_x = (packet[0] & 0x10) ? (int)packet[1] - 256 : packet[1];
    delta_y = (packet[0] & 0x20) ? (int)packet[2] - 256 : packet[2];

    mouse_x += delta_x;
    mouse_y -= delta_y;
    if (mouse_x < 0) {
        mouse_x = 0;
    } else if (mouse_x > mouse_max_x) {
        mouse_x = mouse_max_x;
    }
    if (mouse_y < 0) {
        mouse_y = 0;
    } else if (mouse_y > mouse_max_y) {
        mouse_y = mouse_max_y;
    }

    mouse_buttons = packet[0] & 0x07;
    mouse_generation++;
}

int mouse_poll(struct mouse_state *state) {
    uint32_t generation;

    if (!mouse_ready || state == 0) {
        return 0;
    }

    generation = mouse_generation;
    if (generation == mouse_seen_generation) {
        return 0;
    }
    state->x = mouse_x;
    state->y = mouse_y;
    state->buttons = mouse_buttons;
    mouse_seen_generation = generation;
    return 1;
}
