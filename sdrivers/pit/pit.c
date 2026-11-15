#include <stdint.h>
#include <kernel/ports.h>
#include <kernel/interrupts.h>
#include <kernel/pit.h>

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_MODE_RATE_GENERATOR 0x34
#define PIT_MIN_DIVISOR 1u
#define PIT_MAX_DIVISOR 65535u

static volatile uint64_t pit_ticks;
static volatile uint32_t pit_frequency;
static volatile int pit_initialized;

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
 * Program channel zero in square-wave/rate-generator mode. The divisor
 * is written low byte first, followed by the high byte, as required by
 * the 8253/8254 programming interface.
 */
int pit_set_frequency(uint32_t frequency)
{
    uint32_t divisor = pit_calculate_divisor(frequency);
    uint32_t actual_frequency;

    if (divisor == 0) {
        return -1;
    }

    actual_frequency = PIT_INPUT_HZ / divisor;

    port_byte_out(PIT_COMMAND, PIT_MODE_RATE_GENERATOR);
    port_byte_out(PIT_CHANNEL0, (uint8_t)(divisor & 0xFFu));
    port_byte_out(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFFu));

    pit_frequency = actual_frequency;
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

    if (pit_set_frequency(frequency) != 0) {
        pit_irq_restore(flags);
        return -1;
    }

    pit_initialized = 1;
    interrupts_enable_irq(0);

    pit_irq_restore(flags);
    return 0;
}

/* Called from the common IRQ dispatcher for hardware IRQ0. */
void pit_irq_handler(void)
{
    if (pit_initialized) {
        pit_ticks++;
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
        /* The PIT is not ready yet; use a conservative fallback delay. */
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

    duration = ((uint64_t)milliseconds * frequency + 999u) / 1000u;
    if (duration == 0) {
        duration = 1;
    }

    /*
     * Use HLT only while interrupts are enabled. With IF clear, waiting
     * for an IRQ would deadlock, so use a tick-polled pause loop instead.
     */
    __asm__ volatile ("pushfl; popl %0" : "=r"(flags));
    if (flags & 0x200u) {
        while ((pit_get_ticks() - start) < duration) {
            __asm__ volatile ("hlt");
        }
    } else {
        /*
         * IRQ0 cannot run while IF is clear, so a tick-based wait here
         * would never finish. Preserve the caller's interrupt state.
         */
        pit_fallback_delay(milliseconds);
    }
}
