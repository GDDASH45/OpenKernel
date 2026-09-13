#ifndef USER_H
#define USER_H

#include <stdint.h>

typedef struct user_process {
    uint32_t pid;
    uint32_t esp;
    uint32_t eip;
    int active;
} user_process_t;

void user_init(void);
int user_create_process(uint32_t entry_point);

#endif