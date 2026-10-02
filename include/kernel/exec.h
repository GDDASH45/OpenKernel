#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

int run_binary(uint32_t initrd_start, const char *filename);

#endif