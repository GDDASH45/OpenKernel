#ifndef KERNEL_MODULE_H
#define KERNEL_MODULE_H

#include <stdint.h>
#include <stddef.h>

#define __init __attribute__((__section__(".init.text")))
#define __exit __attribute__((__section__(".exit.text")))

typedef struct kernel_module {
    const char *name;
    int (*init)(void);
    void (*exit)(void);
    uint32_t flags;
} kernel_module_t;

#define KERNEL_MODULE(mod_name, init_fn, exit_fn) \
    static kernel_module_t __mod_##mod_name = { \
        .name = #mod_name, \
        .init = init_fn, \
        .exit = exit_fn, \
        .flags = 0 \
    };

#endif