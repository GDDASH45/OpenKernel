#include <kernel/module.h>
#include <kernel.h>

#define MAX_KERNEL_MODULES 128

static kernel_module_t *module_table[MAX_KERNEL_MODULES];
static size_t module_count;

int __internal_insert_mod(kernel_module_t *mod_addr)
{
    if (mod_addr == NULL) {
        return -1;
    }

    if (module_count >= MAX_KERNEL_MODULES) {
        k_print("[MODULE] Module table full!");
        return -1;
    }

    module_table[module_count] = mod_addr;
    module_count++;

    return 0;
}