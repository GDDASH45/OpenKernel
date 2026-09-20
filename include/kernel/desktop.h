#ifndef DESKTOP_H
#define DESKTOP_H

#include <kernel/multiboot.h>

void desktop_run(const struct multiboot_info *mbi);
void desktop_show(const struct multiboot_info *mbi);

#endif