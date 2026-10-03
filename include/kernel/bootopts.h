#ifndef KERNEL_BOOTOPTS_H
#define KERNEL_BOOTOPTS_H

#include <stdint.h>

void bootopts_init(const char *command_line);
int bootopts_quiet(void);
const char *bootopts_init_path(void);
const char *bootopts_console_path(void);

#endif
