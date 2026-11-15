#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H

#include <stdint.h>

#define GDT_KERNEL_CODE 0x08u
#define GDT_KERNEL_DATA 0x10u
#define GDT_USER_CODE   0x1Bu
#define GDT_USER_DATA   0x23u
#define GDT_TSS         0x28u

void gdt_init(void);
void gdt_set_kernel_stack(uint32_t stack_top);

/*
 * Enter ring 3 using an already mapped user entry point and user stack.
 * Both addresses must refer to pages accessible at CPL3.
 * User code returns through the syscall/exit path; this routine does not return.
 */
void gdt_enter_user_mode(uint32_t entry, uint32_t user_stack)
    __attribute__((noreturn));

#endif
