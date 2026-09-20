#include <stdint.h>
#include <kernel/elf.h>
#include <memory/oom_kill.h>

static int range_is_valid(uint32_t offset, uint32_t size, uint32_t limit) {
    return offset <= limit && size <= limit - offset;
}

int binfmt_elf_load(const void *file_data, uint32_t file_size,
                    void (**entry_out)(void)) {
    const uint8_t *file = (const uint8_t *)file_data;
    const Elf32_Ehdr *elf;

    if (file_data == 0 || entry_out == 0 || file_size < sizeof(Elf32_Ehdr)) {
        return -1;
    }

    elf = (const Elf32_Ehdr *)file;
    if (elf->e_ident != ELF_MAGIC || elf->e_class != 1 || elf->e_data != 1 ||
        elf->e_version != 1 || elf->e_machine != 3 ||
        (elf->e_type != 2 && elf->e_type != 3) ||
        elf->e_phentsize != sizeof(Elf32_Phdr) || elf->e_phnum == 0 ||
        !range_is_valid(elf->e_phoff,
                        (uint32_t)elf->e_phnum * elf->e_phentsize,
                        file_size)) {
        return -2;
    }

    for (uint16_t index = 0; index < elf->e_phnum; index++) {
        const Elf32_Phdr *program =
            (const Elf32_Phdr *)(file + elf->e_phoff + index * elf->e_phentsize);
        uint8_t *destination;

        if (program->p_type != 1) {
            continue;
        }
        if (program->p_filesz > program->p_memsz ||
            !range_is_valid(program->p_offset, program->p_filesz, file_size) ||
            program->p_vaddr + program->p_memsz < program->p_vaddr ||
            check_memory_quota(program->p_vaddr, program->p_memsz) != 0) {
            return -3;
        }

        destination = (uint8_t *)(uintptr_t)program->p_vaddr;
        for (uint32_t byte = 0; byte < program->p_filesz; byte++) {
            destination[byte] = file[program->p_offset + byte];
        }
        for (uint32_t byte = program->p_filesz; byte < program->p_memsz; byte++) {
            destination[byte] = 0;
        }
    }

    *entry_out = (void (*)(void))(uintptr_t)elf->e_entry;
    return 0;
}
