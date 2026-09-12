#ifndef SCRIPT_H
#define SCRIPT_H

#include <stdint.h>

void script_run(const char *script_data, uint32_t size, uint32_t initrd_start);

#endif