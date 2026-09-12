#include <write/write.h>
#include <ksys/version.h>

void info_module_init(void) {
    k_print("----------------------------------------\n");
    k_print("[MODULE] Loaded: System Information\n");
    k_print("[MODULE] OpenKernel extension active.\n");
    k_print("----------------------------------------\n");
    k_clear_screen();
    k_print("----------------------------------------\n");
    k_print("Kernel version: ");
    k_print(K_VER);
}