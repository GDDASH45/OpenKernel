#include <write/write.h>
#include <ksys/version.h>
#include <kernel/time.h>
#include <ksys/system-info.h>

void info_module_init(void) {
    k_print("----------------------------------------\n");
    k_print("[MODULE] Loaded: System Information\n");
    k_print("[MODULE] OpenKernel extension active.\n");
    k_print("----------------------------------------\n");
    sleep_ms(3000);
    k_print("----------------------------------------\n");
    k_print("Kernel version: ");
    k_print(K_VER);
    k_print("----------------------------------------\n");
    sleep_ms(1500);
    k_clear_screen();
}