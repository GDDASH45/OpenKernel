#include <stdint.h>
#include <write/write.h>
#include <kernel/tar.h>

typedef void (*program_entry_t)(void);

void run_binary(uint32_t initrd_start, const char *filename) {
    uint32_t size = 0;
    const char *binary_data = tar_get_file(initrd_start, filename, &size);

    if (!binary_data) {
        k_print("Binary not found: ");
        k_print(filename);
        k_print("\n");
        return;
    }

    k_print("Executing binary: ");
    k_print(filename);
    k_print("\n");

    // Cast the memory address to a function pointer and call it
    program_entry_t entry = (program_entry_t)binary_data;
    entry();

    k_print("\nBinary finished execution.\n");
}