#include <kernel/process.h>
#include <kernel.h>

static uint32_t current_pid;
static uint32_t next_pid;
static uint32_t execution_context[6];
static volatile int execution_context_active;

extern int32_t process_context_save(uint32_t *context);
extern void process_context_restore(uint32_t *context, int32_t status)
    __attribute__((noreturn));

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

int32_t process_execute(int32_t (*entry)(void), int returns_status) {
    int32_t status;

    execution_context_active = 1;
    status = process_context_save(execution_context);
    if (status == 0 && execution_context_active) {
        if (returns_status) {
            status = entry();
        } else {
            ((void (*)(void))entry)();
            status = 0;
        }
        process_exit_current(status);
    }
    execution_context_active = 0;
    return (int32_t)(uint8_t)status;
}

void process_exit_current(int32_t status) {
    if (current_pid == 1) {
        panic("PID 1 (init) exited!");
    }
    if (execution_context_active) {
        execution_context_active = 0;
        process_context_restore(execution_context, (int32_t)(uint8_t)status);
    }
}
