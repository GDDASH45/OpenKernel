
#include <stdint.h>
#include <kernel/module.h>
#include <kernel.h>
#include <generated/version.h>

static int __init version_init(void)
{
    k_print("[MODULE] OpenKernel ");
    k_print(OPENKERNEL_VERSION);
    k_print("\n[MODULE] ");
    k_print(OPENKERNEL_PRETTYNAME);
    k_print("\n");

    return 0;
}

static void __exit version_exit(void)
{
    k_print("[MODULE] Version module unloaded.\n");
}

KERNEL_MODULE(version, version_init, version_exit);