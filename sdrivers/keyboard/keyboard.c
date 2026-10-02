#include <driver/keyboard.h>
#include <kernel/interrupts.h>
#include <kernel/ports.h>
#include <stdint.h>

#define PS2_DATA             0x60
#define PS2_STATUS           0x64
#define PS2_COMMAND          0x64
#define PS2_OUTPUT_FULL      0x01
#define PS2_INPUT_FULL       0x02
#define PS2_AUX_DATA         0x20
#define KEYBOARD_ACK         0xFA
#define KEYBOARD_BUFFER_SIZE 128
#define KEYBOARD_BUFFER_MASK (KEYBOARD_BUFFER_SIZE - 1)

static const char keymap[128] = {
    [0x01] = 27,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
    [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/', [0x37] = '*',
    [0x39] = ' '
};

static const char shifted_keymap[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x1A] = '{', [0x1B] = '}', [0x27] = ':', [0x28] = '"',
    [0x29] = '~', [0x2B] = '|', [0x33] = '<', [0x34] = '>',
    [0x35] = '?'
};

static volatile char character_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint8_t buffer_head;
static volatile uint8_t buffer_tail;
static volatile uint8_t shift_down;
static volatile uint8_t caps_lock;
static volatile uint8_t extended_scancode;
static volatile uint8_t keyboard_ready;

static int wait_for_input_empty(void) {
    uint32_t timeout = 100000;

    while ((port_byte_in(PS2_STATUS) & PS2_INPUT_FULL) != 0 && timeout != 0) {
        timeout--;
    }
    return timeout != 0;
}

static int wait_for_output_full(void) {
    uint32_t timeout = 100000;

    while ((port_byte_in(PS2_STATUS) & PS2_OUTPUT_FULL) == 0 && timeout != 0) {
        timeout--;
    }
    return timeout != 0;
}

static int send_keyboard_command(uint8_t command) {
    if (!wait_for_input_empty()) {
        return 0;
    }
    port_byte_out(PS2_DATA, command);
    if (!wait_for_output_full()) {
        return 0;
    }
    return port_byte_in(PS2_DATA) == KEYBOARD_ACK;
}

static void queue_character(char character) {
    uint8_t next = (uint8_t)((buffer_head + 1) & KEYBOARD_BUFFER_MASK);

    if (next == buffer_tail) {
        return; /* Drop newest input rather than overwrite unread characters. */
    }
    character_buffer[buffer_head] = character;
    buffer_head = next;
}

int init_keyboard(void) {
    uint8_t config;

    keyboard_ready = 0;
    __asm__ volatile ("cli");

    if (!wait_for_input_empty()) {
        return -1;
    }
    port_byte_out(PS2_COMMAND, 0xAD); /* Disable keyboard while configuring. */

    if (!wait_for_input_empty()) {
        return -1;
    }
    port_byte_out(PS2_COMMAND, 0xAA); /* Controller self-test. */
    if (!wait_for_output_full() || port_byte_in(PS2_DATA) != 0x55) {
        return -1;
    }

    /* Read configuration, enable IRQ1, and ensure the keyboard clock is on. */
    if (!wait_for_input_empty()) {
        return -1;
    }
    port_byte_out(PS2_COMMAND, 0x20);
    if (!wait_for_output_full()) {
        return -1;
    }
    config = port_byte_in(PS2_DATA);
    config |= 0x01;
    config &= (uint8_t)~0x10;

    if (!wait_for_input_empty()) {
        return -1;
    }
    port_byte_out(PS2_COMMAND, 0x60);
    if (!wait_for_input_empty()) {
        return -1;
    }
    port_byte_out(PS2_DATA, config);

    if (!wait_for_input_empty()) {
        return -1;
    }
    port_byte_out(PS2_COMMAND, 0xAE); /* Re-enable keyboard port. */

    /* Discard stale controller bytes before enabling scan reporting. */
    while (port_byte_in(PS2_STATUS) & PS2_OUTPUT_FULL) {
        (void)port_byte_in(PS2_DATA);
    }

    if (!send_keyboard_command(0xF4)) { /* Enable keyboard scanning. */
        return -1;
    }

    buffer_head = 0;
    buffer_tail = 0;
    shift_down = 0;
    caps_lock = 0;
    extended_scancode = 0;
    keyboard_ready = 1;
    interrupts_enable_irq(1);
    return 0;
}

void keyboard_irq_handler(void) {
    uint8_t status = port_byte_in(PS2_STATUS);
    uint8_t scancode;
    uint8_t released;
    uint8_t code;
    char character;

    if (!keyboard_ready || (status & (PS2_OUTPUT_FULL | PS2_AUX_DATA)) !=
                           PS2_OUTPUT_FULL) {
        return;
    }

    scancode = port_byte_in(PS2_DATA);
    if (scancode == 0xE0 || scancode == 0xE1) {
        extended_scancode = 1;
        return;
    }
    if (extended_scancode) {
        extended_scancode = 0;
        return; /* Ignore navigation and multimedia keys for now. */
    }

    released = (scancode & 0x80) != 0;
    code = scancode & 0x7F;
    if (code == 0x2A || code == 0x36) {
        shift_down = released ? 0 : 1;
        return;
    }
    if (released) {
        return;
    }
    if (code == 0x3A) {
        caps_lock ^= 1;
        return;
    }
    if (code >= sizeof(keymap) || keymap[code] == 0) {
        return;
    }

    character = keymap[code];
    if ((character >= 'a' && character <= 'z') && (shift_down ^ caps_lock)) {
        character = (char)(character - 'a' + 'A');
    } else if (shift_down && shifted_keymap[code] != 0) {
        character = shifted_keymap[code];
    }
    queue_character(character);
}

int keyboard_try_get_char(char *character) {
    uint32_t flags;

    if (character == 0 || !keyboard_ready) {
        return 0;
    }

    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    if (buffer_tail == buffer_head) {
        __asm__ volatile ("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
        return 0;
    }
    *character = character_buffer[buffer_tail];
    buffer_tail = (uint8_t)((buffer_tail + 1) & KEYBOARD_BUFFER_MASK);
    __asm__ volatile ("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
    return 1;
}

char keyboard_wait_and_get_char(void) {
    char character;

    while (!keyboard_try_get_char(&character)) {
        __asm__ volatile ("pause" ::: "memory");
    }
    return character;
}
