#include <stdint.h>
#include <kernel/kaslr.h>

extern uint8_t __kernel_start;

uint32_t kaslr_get_base(void)
{
    return (uint32_t)(uintptr_t)&__kernel_start;
}

uint32_t kaslr_get_slide(void)
{
    uint32_t base = kaslr_get_base();

    if (base < KASLR_KERNEL_LINK_BASE)
        return 0;
    return base - KASLR_KERNEL_LINK_BASE;
}

int kaslr_is_enabled(void)
{
    return kaslr_get_slide() != 0;
}
