// init/main.c
#include <kernel.h>
#include <write/write.h>

// The built in kernel path variable
char g_kernel_path[128] = "/boot:/root:/device";

void init_path(void)
{
    k_print("[ENV] Initializing default kernel PATH: ");
    k_print(g_kernel_path);
    k_print("\n");
}