#include <stdint.h>
#include <kernel/module.h>
#include <kernel.h>

static int __init hello_init(void)
{
    k_print("[MODULE] Hello from OpenKernel!");
    return 0;
}

static void __exit hello_exit(void)
{
    k_print("[MODULE] Goodbye From OpenKernel!");
}

KERNEL_MODULE(hello, hello_init, hello_exit);