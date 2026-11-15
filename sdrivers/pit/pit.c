#include <stdint.h>
#include <kernel/ports.h>
#include <kernel/interrupts.h>
#include <kernel/pit.h>

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_MODE_RATE_GENERATOR 0x34
#define PIT_MIN_DIVISOR 1u
#define PIT_MAX_DIVISOR 65535u

struct pit_timer {
    uint32_t id;
    uint64_t deadline;
    uint64_t interval;
    pit_timer_callback_t callback;
    void *context;
    uint8_t active;
};

static volatile uint64_t pit_ticks;
static volatile uint32_t pit_frequency;
static volatile int pit_initialized;
static struct pit_timer pit_timers[PIT_TIMER_MAX];
static uint32_t pit_next_timer_id = 1;

static uint32_t pit_irq_save(void)
{
    uint32_t flags;

    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void pit_irq_restore(uint32_t flags)
{
    if (flags & 0x200u) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

static uint64_t pit_ms_to_ticks(uint32_t milliseconds, uint32_t frequency)
{
    uint64_t ticks;

    ticks = ((uint64_t)milliseconds * frequency + 999u) / 1000u;
    return ticks == 0 ? 1 : ticks;
}

static uint32_t pit_calculate_divisor(uint32_t frequency)
{
    uint32_t divisor;

    if (frequency == 0) {
        return 0;
    }

    divisor = (PIT_INPUT_HZ + (frequency / 2u)) / frequency;

    if (divisor < PIT_MIN_DIVISOR || divisor > PIT_MAX_DIVISOR) {
        return 0;
    }

    return divisor;
}

/*
 * Program channel zero in rate-generator mode. The divisor is written
 * low byte first, followed by the high byte.
 */
int pit_set_frequency(uint32_t frequency)
{
    uint32_t divisor = pit_calculate_divisor(frequency);
    uint32_t actual_frequency;
    uint32_t flags;

    if (divisor == 0) {
        return -1;
    }

    flags = pit_irq_save();
    actual_frequency = PIT_INPUT_HZ / divisor;

    port_byte_out(PIT_COMMAND, PIT_MODE_RATE_GENERATOR);
    port_byte_out(PIT_CHANNEL0, (uint8_t)(divisor & 0xFFu));
    port_byte_out(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFFu));

    pit_frequency = actual_frequency;
    pit_irq_restore(flags);
    return 0;
}

int pit_init(uint32_t frequency)
{
    uint32_t flags;

    if (frequency == 0) {
        frequency = PIT_DEFAULT_HZ;
    }

    flags = pit_irq_save();

    pit_ticks = 0;
    pit_initialized = 0;
    pit_next_timer_id = 1;

    for (uint32_t i = 0; i < PIT_TIMER_MAX; i++) {
        pit_timers[i].active = 0;
        pit_timers[i].id = 0;
        pit_timers[i].callback = 0;
        pit_timers[i].context = 0;
        pit_timers[i].deadline = 0;
        pit_timers[i].interval = 0;
    }

    if (pit_set_frequency(frequency) != 0) {
        pit_irq_restore(flags);
        return -1;
    }

    pit_initialized = 1;
    interrupts_enable_irq(0);

    pit_irq_restore(flags);
    return 0;
}

/*
 * Called from the common IRQ dispatcher for hardware IRQ0.
 * Due callbacks are copied out before invocation so callbacks may safely
 * start or cancel timers without changing the scan currently in progress.
 */
void pit_irq_handler(void)
{
    pit_timer_callback_t callbacks[PIT_TIMER_MAX];
    void *contexts[PIT_TIMER_MAX];
    uint32_t callback_count = 0;
    uint64_t now;

    if (!pit_initialized) {
        return;
    }

    now = ++pit_ticks;

    for (uint32_t i = 0; i < PIT_TIMER_MAX; i++) {
        struct pit_timer *timer = &pit_timers[i];

        if (!timer->active || now < timer->deadline) {
            continue;
        }

        callbacks[callback_count] = timer->callback;
        contexts[callback_count] = timer->context;
        callback_count++;

        if (timer->interval != 0) {
            timer->deadline += timer->interval;
        } else {
            timer->active = 0;
        }
    }

    for (uint32_t i = 0; i < callback_count; i++) {
        callbacks[i](contexts[i]);
    }
}

uint64_t pit_get_ticks(void)
{
    uint32_t flags = pit_irq_save();
    uint64_t ticks = pit_ticks;

    pit_irq_restore(flags);
    return ticks;
}

uint32_t pit_get_frequency(void)
{
    uint32_t flags = pit_irq_save();
    uint32_t frequency = pit_frequency;

    pit_irq_restore(flags);
    return frequency;
}

int pit_is_initialized(void)
{
    return pit_initialized != 0;
}

/*
 * Convert ticks using the actual programmed frequency rather than the
 * requested frequency. This avoids accumulating divisor-rounding error.
 */
uint64_t pit_get_uptime_ms(void)
{
    uint64_t ticks;
    uint32_t frequency;

    ticks = pit_get_ticks();
    frequency = pit_get_frequency();

    if (frequency == 0) {
        return 0;
    }

    return (ticks * 1000u) / frequency;
}

static void pit_fallback_delay(uint32_t milliseconds)
{
    volatile uint32_t outer;
    volatile uint32_t inner;

    for (outer = 0; outer < milliseconds; outer++) {
        for (inner = 0; inner < 40000u; inner++) {
            __asm__ volatile ("nop");
        }
    }
}

void pit_sleep_ms(uint32_t milliseconds)
{
    uint64_t start;
    uint64_t duration;
    uint32_t frequency;
    uint32_t flags;

    if (milliseconds == 0) {
        return;
    }

    if (!pit_is_initialized()) {
        pit_fallback_delay(milliseconds);
        return;
    }

    flags = pit_irq_save();
    frequency = pit_frequency;
    start = pit_ticks;
    pit_irq_restore(flags);

    if (frequency == 0) {
        return;
    }

    duration = pit_ms_to_ticks(milliseconds, frequency);

    __asm__ volatile ("pushfl; popl %0" : "=r"(flags));
    if (flags & 0x200u) {
        while ((pit_get_ticks() - start) < duration) {
            __asm__ volatile ("hlt");
        }
    } else {
        /* IRQ0 cannot advance while IF is clear. */
        pit_fallback_delay(milliseconds);
    }
}

/*
 * Schedule a one-shot or periodic callback. The timer table is fixed-size
 * so scheduling never allocates memory and remains usable during boot.
 */
uint32_t pit_timer_start(uint32_t delay_ms, uint32_t interval_ms,
                         pit_timer_callback_t callback, void *context)
{
    uint32_t flags;
    uint32_t frequency;
    uint32_t id;
    uint64_t delay_ticks;
    uint64_t interval_ticks;

    if (callback == 0 || !pit_initialized) {
        return 0;
    }

    flags = pit_irq_save();
    frequency = pit_frequency;
    if (frequency == 0) {
        pit_irq_restore(flags);
        return 0;
    }

    delay_ticks = pit_ms_to_ticks(delay_ms, frequency);
    interval_ticks = interval_ms == 0
        ? 0 : pit_ms_to_ticks(interval_ms, frequency);

    for (uint32_t i = 0; i < PIT_TIMER_MAX; i++) {
        if (pit_timers[i].active) {
            continue;
        }

        id = pit_next_timer_id++;
        if (id == 0) {
            id = pit_next_timer_id++;
        }

        pit_timers[i].id = id;
        pit_timers[i].deadline = pit_ticks + delay_ticks;
        pit_timers[i].interval = interval_ticks;
        pit_timers[i].callback = callback;
        pit_timers[i].context = context;
        pit_timers[i].active = 1;

        pit_irq_restore(flags);
        return id;
    }

    pit_irq_restore(flags);
    return 0;
}

int pit_timer_cancel(uint32_t timer_id)
{
    uint32_t flags;

    if (timer_id == 0) {
        return -1;
    }

    flags = pit_irq_save();
    for (uint32_t i = 0; i < PIT_TIMER_MAX; i++) {
        if (pit_timers[i].active && pit_timers[i].id == timer_id) {
            pit_timers[i].active = 0;
            pit_irq_restore(flags);
            return 0;
        }
    }

    pit_irq_restore(flags);
    return -1;
}

uint32_t pit_timer_active_count(void)
{
    uint32_t flags = pit_irq_save();
    uint32_t count = 0;

    for (uint32_t i = 0; i < PIT_TIMER_MAX; i++) {
        if (pit_timers[i].active) {
            count++;
        }
    }

    pit_irq_restore(flags);
    return count;
}
