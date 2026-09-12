#include <driver/keyboard.h>
#include <kernel/ports.h>
#include <write/write.h>

// US QWERTY Scancode Set 1 translation table (pressed keys)
static const char scancode_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

int init_keyboard(void) {
    // Wait for input buffer to clear
    int timeout = 100000;
    while ((port_byte_in(0x64) & 0x02) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) return -1;

    // Send self-test command to PS/2 controller
    port_byte_out(0x64, 0xAA);

    // Wait for response in output buffer
    timeout = 100000;
    while (!(port_byte_in(0x64) & 0x01) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) return -1;

    // Controller should reply with 0x55 on success
    uint8_t response = port_byte_in(0x60);
    if (response != 0x55) {
        return -1;
    }

    k_print("PS/2 Keyboard controller verified.\n");
    return 0;
}

char keyboard_wait_and_get_char(void) {
    while (1) {
        // Check if output buffer has data (Port 0x64, bit 0)
        if (port_byte_in(0x64) & 0x01) {
            uint8_t scancode = port_byte_in(0x60);
            
            // Ignore key releases (break codes have high bit set: >= 0x80)
            if (scancode & 0x80) {
                continue;
            }

            if (scancode < sizeof(scancode_ascii)) {
                char ascii = scancode_ascii[scancode];
                if (ascii != 0) {
                    return ascii;
                }
            }
        }
    }
}