#include <kernel/module.h>
#include <write/write.h>
#include <kernel.h>

static int __init ram_init(void)
{
    k_print("[MODULE] RAM subsystem module loaded successfully.\n");
    // Initialize RAM tracking here
    return 0;
}

static void __exit ram_exit(void)
{
    k_print("[MODULE] RAM subsystem moduke unloaded.\n");
}

KERNEL_MODULE(ram, ram_init, ram_exit);