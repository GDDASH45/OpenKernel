#include <stdint.h>
#include <write/write.h>
#include <kernel/tar.h>
#include <kernel/elf.h>
#include <kernel/binfmt_okx.h>
#include <kernel/process.h>
#include "../../fs/binfmt_pmx.h"

typedef int32_t (*entry_point_t)(void);

// Store initramfs start address globally during kernel init
static uint32_t g_initrd_start = 0;

void initrd_set_base(uint32_t addr) {
    g_initrd_start = addr;
    process_system_init();
}

int32_t process_load_oso(const char *path) {
    uint32_t file_size = 0;
    const char *file_data;
    void (*entry)(void) = 0;

    if (path == 0 || path[0] == '\0' || g_initrd_start == 0) {
        return -1;
    }

    file_data = tar_get_file(g_initrd_start, path, &file_size);
    if (file_data == 0 || binfmt_oso_load(file_data, file_size, &entry) != 0) {
        return -1;
    }
    return (int32_t)(uintptr_t)entry;
}

int execve(const char *filename, char *const argv[], char *const envp[]) {
    const char *archive_name = filename;

    (void)argv;
    (void)envp;

    if (filename == 0 || filename[0] == '\0') {
        return -1;
    }
    while (*archive_name == '/') {
        archive_name++;
    }
    if (*archive_name == '\0') {
        return -1;
    }

    k_print("execve: Locating binary '");
    k_print(archive_name);
    k_print("' in initramfs...\n");

    if (g_initrd_start == 0) {
        k_print("execve: Initramfs base address not set!\n");
        return -1;
    }

    uint32_t file_size = 0;
    const char *file_data = tar_get_file(g_initrd_start, archive_name, &file_size);

    if (!file_data) {
        k_print("execve: Binary not found in archive.\n");
        return -1;
    }

    entry_point_t program = 0;
    void (*loaded_program)(void) = 0;
    int load_result;
    int returns_status = 0;

    if (file_size >= 4 && *(const uint32_t *)file_data == 0x31584B4F) {
        load_result = binfmt_okx_load(file_data, file_size, &loaded_program);
        returns_status = 1;
    } else if (file_size >= 4 && *(const uint32_t *)file_data == ELF_MAGIC) {
        load_result = binfmt_elf_load(file_data, file_size, &loaded_program);
    } else if (file_size >= 4 && *(const uint32_t *)file_data == 0x31584D50) {
        load_result = binfmt_pmx_load(file_data, file_size, &loaded_program);
    } else {
        uint8_t *dest = (uint8_t *)0x200000;
        const uint8_t *src = (const uint8_t *)file_data;

        if (file_size > 0x100000) {
            return -1;
        }
        for (uint32_t i = 0; i < file_size; i++) {
            dest[i] = src[i];
        }
        program = (entry_point_t)dest;
        load_result = 0;
    }

    if (load_result == 0 && loaded_program != 0) {
        program = (entry_point_t)loaded_program;
    }
    if (load_result != 0 || program == 0) {
        k_print("execve: Unsupported or invalid executable format.\n");
        return -1;
    }

    k_print("execve: Jumping to program entry point...\n");
    int32_t exit_status;
    if (returns_status) {
        exit_status = program();
    } else {
        ((void (*)(void))program)();
        exit_status = 0;
    }
    process_exit_current(exit_status);
    return (int32_t)(uint8_t)exit_status;
}