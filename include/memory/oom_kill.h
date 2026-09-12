#ifndef OOM_KILL_H
#define OOM_KILL_H

#include <stdint.h>

void oom_kill(const char *process_name, uint32_t requested_addr, uint32_t size);
int check_memory_quota(uint32_t addr, uint32_t size);

#endif