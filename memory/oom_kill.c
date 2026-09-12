#include <stdint.h>
#include <kernel.h>
#include <write/write.h>

#define MAX_SYSTEM_RAM 0x01000000

void oom_kill(const char *process_name, uint32_t requested_addr, uint32_t size) {
    k_print("[OOM] Memory limit exceeded by process: ");
    k_print(process_name);
    k_print("\n[OOM] Requested Address: 0x");

    k_print("\n");

    panic("OOM Killer: Program attempted to overrun physical memory bounds.");
}

int check_memory_quota(uint32_t addr, uint32_t size) {
    uint32_t end_addr = addr + size;

    if (end_addr < addr || end_addr > MAX_SYSTEM_RAM) {
        return -1;
    }

    return 0;
}