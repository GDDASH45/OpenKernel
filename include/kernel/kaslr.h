#ifndef KERNEL_KASLR_H
#define KERNEL_KASLR_H

#include <stdint.h>

#define KASLR_KERNEL_LINK_BASE 0x00400000u

uint32_t kaslr_get_slide(void);
uint32_t kaslr_get_base(void);
int kaslr_is_enabled(void);

#endif
