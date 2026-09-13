#include <user/user.h>
#include <write/write.h>

static user_process_t current_user_process;

void user_init(void) {
    k_print("[USER] Initializing user-space subsystem\n");
    current_user_process.pid = 0;
    current_user_process.active = 0;
}

int user_create_process(uint32_t entry_point) {
    k_print("[USER] Spawning user process...\n");
    current_user_process.pid = 1;
    current_user_process.eip = entry_point;
    current_user_process.active = 1;
    return current_user_process.pid;
}