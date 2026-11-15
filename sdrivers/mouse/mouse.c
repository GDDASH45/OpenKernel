#include <driver/mouse.h>
#include <kernel/interrupts.h>
#include <kernel/ports.h>

#define PS2_DATA              0x60
#define PS2_STATUS            0x64
#define PS2_COMMAND           0x64
#define PS2_OUTPUT_FULL       0x01
#define PS2_INPUT_FULL        0x02
#define PS2_AUX_DATA          0x20
#define PS2_ENABLE_AUX        0xA8
#define PS2_READ_CONFIG       0x20
#define PS2_WRITE_CONFIG      0x60
#define PS2_WRITE_AUX         0xD4

#define MOUSE_ACK             0xFA
#define MOUSE_RESEND          0xFE
#define MOUSE_GET_ID          0xF2
#define MOUSE_SET_DEFAULTS    0xF6
#define MOUSE_ENABLE          0xF4
#define MOUSE_SET_SAMPLE_RATE 0xF3

#define MOUSE_PACKET_STANDARD 3
#define MOUSE_PACKET_WHEEL    4
#define PS2_TIMEOUT           100000u
#define MOUSE_COMMAND_RETRIES 3u
#define MOUSE_IRQ_DRAIN_LIMIT 16u

static volatile struct mouse_state current_state;
static volatile uint32_t mouse_generation;
static uint32_t seen_generation;
static int32_t max_x;
static int32_t max_y;
static uint8_t packet[4];
static uint8_t packet_index;
static uint8_t packet_length;
static uint8_t device_id;
static int mouse_ready;

static uint32_t irq_save(void)
{
    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0; cli"
                      : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(uint32_t flags)
{
    __asm__ volatile ("pushl %0; popfl"
                      : : "r"(flags) : "memory", "cc");
}

static int wait_for_input_empty(void)
{
    uint32_t timeout = PS2_TIMEOUT;
    while (timeout != 0) {
        if ((port_byte_in(PS2_STATUS) & PS2_INPUT_FULL) == 0) return 1;
        timeout--;
    }
    return 0;
}

static int wait_for_output_full(void)
{
    uint32_t timeout = PS2_TIMEOUT;
    while (timeout != 0) {
        if (port_byte_in(PS2_STATUS) & PS2_OUTPUT_FULL) return 1;
        timeout--;
    }
    return 0;
}

static int wait_for_aux_output(void)
{
    uint32_t timeout = PS2_TIMEOUT;
    while (timeout != 0) {
        uint8_t status = port_byte_in(PS2_STATUS);
        if ((status & (PS2_OUTPUT_FULL | PS2_AUX_DATA)) ==
            (PS2_OUTPUT_FULL | PS2_AUX_DATA)) return 1;
        timeout--;
    }
    return 0;
}

static int controller_command(uint8_t command)
{
    if (!wait_for_input_empty()) return 0;
    port_byte_out(PS2_COMMAND, command);
    return 1;
}

static int controller_read_config(uint8_t *config)
{
    if (config == 0 || !controller_command(PS2_READ_CONFIG) ||
        !wait_for_output_full()) return 0;
    *config = port_byte_in(PS2_DATA);
    return 1;
}

static int controller_write_config(uint8_t config)
{
    if (!controller_command(PS2_WRITE_CONFIG) || !wait_for_input_empty())
        return 0;
    port_byte_out(PS2_DATA, config);
    return 1;
}

static int mouse_read_response(uint8_t *value)
{
    if (value == 0 || !wait_for_aux_output()) return 0;
    *value = port_byte_in(PS2_DATA);
    return 1;
}

static int mouse_write_byte(uint8_t value)
{
    if (!controller_command(PS2_WRITE_AUX) || !wait_for_input_empty())
        return 0;
    port_byte_out(PS2_DATA, value);
    return 1;
}

static int mouse_send_byte(uint8_t value)
{
    for (uint32_t attempt = 0; attempt < MOUSE_COMMAND_RETRIES; attempt++) {
        uint8_t response;
        if (!mouse_write_byte(value) || !mouse_read_response(&response))
            return 0;
        if (response == MOUSE_ACK) return 1;
        if (response != MOUSE_RESEND) return 0;
    }
    return 0;
}

static int mouse_send_sample_rate(uint8_t rate)
{
    return mouse_send_byte(MOUSE_SET_SAMPLE_RATE) && mouse_send_byte(rate);
}

static int mouse_get_device_id(uint8_t *id)
{
    return mouse_send_byte(MOUSE_GET_ID) && mouse_read_response(id);
}

static void mouse_try_wheel_mode(void)
{
    uint8_t id = 0;

    /* IntelliMouse wheel-mode negotiation: sample rates 200, 100, 80. */
    if (!mouse_send_sample_rate(200) ||
        !mouse_send_sample_rate(100) ||
        !mouse_send_sample_rate(80) ||
        !mouse_get_device_id(&id)) return;

    device_id = id;
    if (id == 3 || id == 4) packet_length = MOUSE_PACKET_WHEEL;
}

static int32_t clamp_coordinate(int32_t value, int32_t maximum)
{
    if (value < 0) return 0;
    if (value > maximum) return maximum;
    return value;
}

static int32_t clamp_motion(int32_t position, int32_t delta,
                            int32_t maximum)
{
    if (delta > 0 && position > maximum - delta) return maximum;
    if (delta < 0 && position < -delta) return 0;
    return position + delta;
}

static int32_t saturating_add(int32_t left, int32_t right)
{
    if (right > 0 && left > 0x7FFFFFFF - right) return 0x7FFFFFFF;
    if (right < 0 && left < (-0x7FFFFFFF - 1) - right)
        return (-0x7FFFFFFF - 1);
    return left + right;
}

static int32_t signed_wheel_delta(uint8_t value)
{
    value &= 0x0F;
    return (value & 0x08) ? (int32_t)value - 16 : (int32_t)value;
}

static void publish_packet(void)
{
    uint8_t flags = packet[0];
    uint8_t buttons = flags & 0x07;
    uint8_t pressed;
    uint8_t released;
    int32_t dx = (flags & 0x40) ? 0 : (int32_t)(int8_t)packet[1];
    int32_t dy = (flags & 0x80) ? 0 : -(int32_t)(int8_t)packet[2];
    int32_t wheel = 0;

    if (packet_length == MOUSE_PACKET_WHEEL) {
        wheel = signed_wheel_delta(packet[3]);
        if (device_id == 4) {
            if (packet[3] & 0x10) buttons |= MOUSE_BUTTON_4;
            if (packet[3] & 0x20) buttons |= MOUSE_BUTTON_5;
        }
    }

    pressed = (uint8_t)(buttons & (uint8_t)~current_state.buttons);
    released = (uint8_t)(current_state.buttons & (uint8_t)~buttons);

    current_state.x = clamp_motion(current_state.x, dx, max_x);
    current_state.y = clamp_motion(current_state.y, dy, max_y);
    current_state.buttons = buttons;
    current_state.pressed |= pressed;
    current_state.released |= released;
    current_state.delta_x = saturating_add(current_state.delta_x, dx);
    current_state.delta_y = saturating_add(current_state.delta_y, dy);
    current_state.wheel = saturating_add(current_state.wheel, wheel);
    current_state.packets++;
    mouse_generation++;
}

int init_mouse(uint32_t screen_width, uint32_t screen_height)
{
    uint8_t config;

    if (screen_width == 0 || screen_height == 0 ||
        screen_width > 0x7FFFFFFFu || screen_height > 0x7FFFFFFFu)
        return -1;

    if (mouse_ready) {
        mouse_set_bounds(screen_width, screen_height);
        return 0;
    }

    if (!controller_command(PS2_ENABLE_AUX) ||
        !controller_read_config(&config)) return -1;

    /* Enable IRQ12 and the auxiliary clock without changing other bits. */
    config |= 0x02;
    config &= (uint8_t)~0x20;
    if (!controller_write_config(config)) return -1;

    if (!mouse_send_byte(MOUSE_SET_DEFAULTS)) return -1;

    device_id = 0;
    packet_length = MOUSE_PACKET_STANDARD;
    mouse_try_wheel_mode();

    if (!mouse_send_byte(MOUSE_ENABLE)) return -1;

    max_x = (int32_t)screen_width - 1;
    max_y = (int32_t)screen_height - 1;
    current_state.x = max_x / 2;
    current_state.y = max_y / 2;
    current_state.buttons = 0;
    current_state.pressed = 0;
    current_state.released = 0;
    current_state.delta_x = 0;
    current_state.delta_y = 0;
    current_state.wheel = 0;
    current_state.packets = 0;

    packet_index = 0;
    mouse_generation = 0;
    seen_generation = 0;
    mouse_ready = 1;

    interrupts_enable_irq(12);
    return 0;
}

int mouse_is_ready(void)
{
    return mouse_ready;
}

void mouse_set_bounds(uint32_t screen_width, uint32_t screen_height)
{
    uint32_t flags;

    if (screen_width == 0 || screen_height == 0 ||
        screen_width > 0x7FFFFFFFu || screen_height > 0x7FFFFFFFu)
        return;

    flags = irq_save();
    max_x = (int32_t)screen_width - 1;
    max_y = (int32_t)screen_height - 1;
    current_state.x = clamp_coordinate(current_state.x, max_x);
    current_state.y = clamp_coordinate(current_state.y, max_y);
    irq_restore(flags);
}

void mouse_set_position(int32_t x, int32_t y)
{
    uint32_t flags = irq_save();
    current_state.x = clamp_coordinate(x, max_x);
    current_state.y = clamp_coordinate(y, max_y);
    mouse_generation++;
    irq_restore(flags);
}

void mouse_irq_handler(void)
{
    /* Drain several pending bytes, but keep interrupt work strictly bounded. */
    for (uint32_t drained = 0; drained < MOUSE_IRQ_DRAIN_LIMIT; drained++) {
        uint8_t status = port_byte_in(PS2_STATUS);
        uint8_t data;

        if ((status & (PS2_OUTPUT_FULL | PS2_AUX_DATA)) !=
            (PS2_OUTPUT_FULL | PS2_AUX_DATA)) break;

        data = port_byte_in(PS2_DATA);
        if (!mouse_ready) continue;

        if (packet_index == 0) {
            /* Bit 3 is always set in a valid first byte. */
            if ((data & 0x08) == 0) continue;
            packet[0] = data;
            packet_index = 1;
            continue;
        }

        packet[packet_index++] = data;
        if (packet_index < packet_length) continue;

        packet_index = 0;
        publish_packet();
    }
}

int mouse_poll(struct mouse_state *state)
{
    uint32_t flags;
    uint32_t generation;

    if (state == 0 || !mouse_ready) return 0;

    flags = irq_save();
    generation = mouse_generation;
    if (generation == seen_generation) {
        irq_restore(flags);
        return 0;
    }

    state->x = current_state.x;
    state->y = current_state.y;
    state->buttons = current_state.buttons;
    state->pressed = current_state.pressed;
    state->released = current_state.released;
    state->delta_x = current_state.delta_x;
    state->delta_y = current_state.delta_y;
    state->wheel = current_state.wheel;
    state->packets = current_state.packets;

    /* Event fields accumulate until userspace/kernel code polls them. */
    current_state.pressed = 0;
    current_state.released = 0;
    current_state.delta_x = 0;
    current_state.delta_y = 0;
    current_state.wheel = 0;
    current_state.packets = 0;
    seen_generation = generation;

    irq_restore(flags);
    return 1;
}
