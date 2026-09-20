#include <stdint.h>
#include <write/write.h>
#include <kernel/tar.h>
#include <kernel/elf.h>
#include "../fs/binfmt_pmx.h"

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

    program_entry_t entry = 0;
    int load_result;

    if (size >= 4 && *(const uint32_t *)binary_data == ELF_MAGIC) {
        load_result = binfmt_elf_load(binary_data, size, &entry);
    } else if (size >= 4 && *(const uint32_t *)binary_data == 0x31584D50) {
        load_result = binfmt_pmx_load(binary_data, size, &entry);
    } else {
        entry = (program_entry_t)binary_data;
        load_result = 0;
    }

    if (load_result != 0 || entry == 0) {
        k_print("Unsupported or invalid executable format.\n");
        return;
    }

    entry();

    k_print("\nBinary finished execution.\n");
}