#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stdint.h>

void process_system_init(void);
uint32_t process_current_pid(void);
uint32_t process_enter_program(void);
void process_leave_program(uint32_t previous_pid);

#endif
