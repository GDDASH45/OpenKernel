#include <stdint.h>
#include <write/write.h>
#include <kernel/elf.h>

int check_elf_compatibility(const void *file_data) {
    const Elf32_Ehdr *elf = (const Elf32_Ehdr *)file_data;

    if (elf->e_ident == ELF_MAGIC) {
        k_print("This ELF Executable cannot be ran on This OS\n");
        return -1;
    }

    return 0;
}