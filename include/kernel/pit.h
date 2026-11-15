#ifndef KERNEL_PIT_H
#define KERNEL_PIT_H

#include <stdint.h>

/* The PC/AT PIT input clock is approximately 1.193182 MHz. */
#define PIT_INPUT_HZ 1193182u
#define PIT_DEFAULT_HZ 1000u

int pit_init(uint32_t frequency);
int pit_set_frequency(uint32_t frequency);
void pit_irq_handler(void);
uint64_t pit_get_ticks(void);
uint64_t pit_get_uptime_ms(void);
void pit_sleep_ms(uint32_t milliseconds);
uint32_t pit_get_frequency(void);
int pit_is_initialized(void);

#endif
