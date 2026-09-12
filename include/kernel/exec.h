#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

void run_binary(uint32_t initrd_start, const char *filename);

#endif