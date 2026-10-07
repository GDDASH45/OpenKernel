#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stdint.h>

void process_system_init(void);
uint32_t process_current_pid(void);
uint32_t process_enter_program(void);
void process_leave_program(uint32_t previous_pid);
int32_t process_execute(int32_t (*entry)(void), int returns_status);
void process_exit_current(int32_t status);
int32_t process_load_oso(const char *path);

#endif
