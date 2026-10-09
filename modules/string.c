#include <stdint.h>
#include <kernel/module.h>
#include <kernel.h>

static int __init string_init(void)
{
    k_print("[MODULE] String module initialized!");
    return 0;
}

static void __exit string_exit(void)
{
    k_print("[MODULE] String module unloaded!");
}

KERNEL_MODULE(string, string_init, string_exit);