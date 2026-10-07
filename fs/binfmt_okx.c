#include <stdint.h>
#include <kernel/binfmt_okx.h>
#include <memory/oom_kill.h>

#define OSO_LOAD_ADDRESS 0x00300000u
#define OSO_MAX_IMAGE_SIZE 0x00100000u

struct okx_header {
    char magic[4];
    uint32_t load_addr;
    uint32_t entry_offset;
    uint32_t code_size;
} __attribute__((packed));

int binfmt_okx_load(const void *file_data, uint32_t file_size,
                    void (**entry_out)(void)) {
    const struct okx_header *header = (const struct okx_header *)file_data;
    const uint8_t *source;
    uint8_t *destination;

    if (file_data == 0 || entry_out == 0 || file_size < sizeof(*header) ||
        header->magic[0] != 'O' || header->magic[1] != 'K' ||
        header->magic[2] != 'X' || header->magic[3] != '1' ||
        header->entry_offset >= header->code_size ||
        header->code_size > file_size - sizeof(*header) ||
        header->load_addr + header->code_size < header->load_addr ||
        check_memory_quota(header->load_addr, header->code_size) != 0) {
        return -1;
    }

    source = (const uint8_t *)file_data + sizeof(*header);
    destination = (uint8_t *)(uintptr_t)header->load_addr;
    for (uint32_t byte = 0; byte < header->code_size; byte++) {
        destination[byte] = source[byte];
    }

    *entry_out = (void (*)(void))(uintptr_t)(header->load_addr + header->entry_offset);
    return 0;
}

int binfmt_oso_load(const void *file_data, uint32_t file_size,
                    void (**entry_out)(void)) {
    const struct okx_header *header = (const struct okx_header *)file_data;
    const uint8_t *source;
    uint8_t *destination;

    if (file_data == 0 || entry_out == 0 || file_size < sizeof(*header) ||
        header->magic[0] != 'O' || header->magic[1] != 'S' ||
        header->magic[2] != 'O' || header->magic[3] != '1' ||
        header->load_addr != OSO_LOAD_ADDRESS ||
        header->entry_offset >= header->code_size ||
        header->code_size > OSO_MAX_IMAGE_SIZE ||
        header->code_size > file_size - sizeof(*header) ||
        header->load_addr + header->code_size < header->load_addr ||
        check_memory_quota(header->load_addr, header->code_size) != 0) {
        return -1;
    }

    source = (const uint8_t *)file_data + sizeof(*header);
    destination = (uint8_t *)(uintptr_t)header->load_addr;
    for (uint32_t byte = 0; byte < header->code_size; byte++) {
        destination[byte] = source[byte];
    }

    *entry_out = (void (*)(void))(uintptr_t)(header->load_addr + header->entry_offset);
    return 0;
}