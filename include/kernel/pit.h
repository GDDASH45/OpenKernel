#ifndef KERNEL_PIT_H
#define KERNEL_PIT_H

#include <stdint.h>

/* The PC/AT PIT input clock is approximately 1.193182 MHz. */
#define PIT_INPUT_HZ 1193182u
#define PIT_DEFAULT_HZ 1000u
#define PIT_TIMER_MAX 32u

typedef void (*pit_timer_callback_t)(void *context);

int pit_init(uint32_t frequency);
int pit_set_frequency(uint32_t frequency);
void pit_irq_handler(void);
uint64_t pit_get_ticks(void);
uint64_t pit_get_uptime_ms(void);
void pit_sleep_ms(uint32_t milliseconds);
uint32_t pit_get_frequency(void);
int pit_is_initialized(void);

/*
 * Software timers use PIT ticks and are serviced from IRQ0.
 * A zero interval creates a one-shot timer; a nonzero interval repeats.
 * Callbacks execute in interrupt context and must not block or sleep.
 * Returns a nonzero timer ID, or zero if the request cannot be scheduled.
 */
uint32_t pit_timer_start(uint32_t delay_ms, uint32_t interval_ms,
                         pit_timer_callback_t callback, void *context);
int pit_timer_cancel(uint32_t timer_id);
uint32_t pit_timer_active_count(void);

#endif
