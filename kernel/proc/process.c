#include <kernel/process.h>

static uint32_t current_pid;
static uint32_t next_pid;

void process_system_init(void) {
    current_pid = 0;
    next_pid = 1;
}

uint32_t process_current_pid(void) {
    return current_pid;
}

uint32_t process_enter_program(void) {
    uint32_t previous_pid = current_pid;

    current_pid = next_pid++;
    if (next_pid == 0) {
        next_pid = 1;
    }
    return previous_pid;
}

void process_leave_program(uint32_t previous_pid) {
    current_pid = previous_pid;
}
