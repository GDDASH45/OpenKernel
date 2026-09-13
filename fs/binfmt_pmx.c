#include "binfmt_pmx.h"
#include <stdint.h>
#include <memory/oom_kill.h>

struct pmx_header {
    char magic[4];          // "PMX1"
    uint32_t entry_offset;  // Entry point offset relative to load address
    uint32_t code_size;     // Size of the flat binary payload
    uint32_t load_addr;     // Target memory address for the flat binary
} __attribute__((packed));

int binfmt_pmx_load(const void *file_data, uint32_t file_size, void (**entry_out)(void)) {
    if (file_size < sizeof(struct pmx_header)) {
        return -1; // Header bounds check failed
    }

    const struct pmx_header *header = (const struct pmx_header *)file_data;
    if (header->magic[0] != 'P' || header->magic[1] != 'M' || 
        header->magic[2] != 'X' || header->magic[3] != '1') {
        return -2; // Invalid magic bytes
    }

    // Enforce memory quotas before touching destination RAM
    if (check_memory_quota(header->load_addr, header->code_size) != 0) {
        oom_kill("PMX Binary", header->load_addr, header->code_size);
    }

    const char *flat_binary_src = (const char *)file_data + sizeof(struct pmx_header);
    if (sizeof(struct pmx_header) + header->code_size > file_size) {
        return -3; // Payload size mismatch
    }

    // Treat the destination target as a flat memory space
    char *flat_binary_dest = (char *)header->load_addr;
    
    // Copy flat binary payload bytes to the designated load address
    for (uint32_t i = 0; i < header->code_size; i++) {
        flat_binary_dest[i] = flat_binary_src[i];
    }

    // Set the executable entry point pointer
    *entry_out = (void (*)(void))(header->load_addr + header->entry_offset);
    return 0;
}