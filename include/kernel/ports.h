#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

static inline uint8_t port_byte_in(uint16_t port) {
    uint8_t result;
    __asm__("inb %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

static inline void port_byte_out(uint16_t port, uint8_t data) {
    __asm__("outb %0, %1" : : "a" (data), "Nd" (port));
}

#endif