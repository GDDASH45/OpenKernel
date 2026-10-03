#include <stdint.h>
#include <write/write.h>
#include <kernel/tar.h>
#include <kernel/elf.h>
#include <kernel/binfmt_okx.h>
#include <kernel/process.h>
#include "../fs/binfmt_pmx.h"

typedef int32_t (*program_entry_t)(void);

int run_binary(uint32_t initrd_start, const char *filename) {
    uint32_t size = 0;
    const char *binary_data = tar_get_file(initrd_start, filename, &size);

    if (!binary_data) {
        k_print("Binary not found: ");
        k_print(filename);
        k_print("\n");
        return -1;
    }

    k_print("Executing binary: ");
    k_print(filename);
    k_print("\n");

    program_entry_t entry = 0;
    void (*loaded_entry)(void) = 0;
    int load_result;
    int returns_status = 0;

    if (size >= 4 && *(const uint32_t *)binary_data == 0x31584B4F) {
        load_result = binfmt_okx_load(binary_data, size, &loaded_entry);
        returns_status = 1;
    } else if (size >= 4 && *(const uint32_t *)binary_data == ELF_MAGIC) {
        load_result = binfmt_elf_load(binary_data, size, &loaded_entry);
    } else if (size >= 4 && *(const uint32_t *)binary_data == 0x31584D50) {
        load_result = binfmt_pmx_load(binary_data, size, &loaded_entry);
    } else {
        entry = (program_entry_t)binary_data;
        load_result = 0;
    }

    if (load_result == 0 && loaded_entry != 0) {
        entry = (program_entry_t)loaded_entry;
    }
    if (load_result != 0 || entry == 0) {
        k_print("Unsupported or invalid executable format.\n");
        return -1;
    }

    uint32_t previous_pid = process_enter_program();
    int32_t exit_status = process_execute(entry, returns_status);
    process_leave_program(previous_pid);

    k_print("\n[PROC] Program exited with status ");
    if (exit_status >= 100) {
        k_print_char((char)('0' + exit_status / 100));
    }
    if (exit_status >= 10) {
        k_print_char((char)('0' + (exit_status / 10) % 10));
    }
    k_print_char((char)('0' + exit_status % 10));
    k_print(".\n");
    return exit_status;
}