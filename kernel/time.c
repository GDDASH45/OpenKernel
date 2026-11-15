#include <stdint.h>
#include <kernel/time.h>
#include <kernel/pit.h>

void sleep_ms(uint32_t ms)
{
    pit_sleep_ms(ms);
}
