#include <kernel/tar.h>
#include <kernel/script.h>
#include <write/write.h>
#include <kernel.h>

static uint32_t tar_to_int(const char *in) {
    uint32_t size = 0;
    uint32_t count = 1;
    for (int j = 11; j > 0; j--, count *= 8) {
        if (in[j - 1] >= '0' && in[j - 1] <= '7') {
            size += ((in[j - 1] - '0') * count);
        }
    }
    return size;
}

// Check if filename contains or ends with "init"
static int is_init_file(const char *name) {
    int len = 0;
    while (name[len] != '\0' && len < 100) len++;
    
    if (len >= 4) {
        // Check exact match of the tail "init"
        if (name[len-4] == 'i' && name[len-3] == 'n' && 
            name[len-2] == 'i' && name[len-1] == 't') {
            // Ensure it's either "init" or preceded by a path separator '/'
            if (len == 4 || name[len-5] == '/') {
                return 1;
            }
        }
    }
    return 0;
}

void tar_parse(uint32_t address) {
    struct tar_header *header = (struct tar_header *)address;
    int init_found = 0;

    while (header->name[0] != '\0') {
        uint32_t size = tar_to_int(header->size);
        uint32_t file_data_address = address + 512;

        if (is_init_file(header->name)) {
            k_print("Found init script: ");
            k_print(header->name);
            k_print("\n");
            script_run((const char *)file_data_address, size);
            init_found = 1;
            break;
        }

        uint32_t blocks = (size + 512 - 1) / 512;
        address += (blocks + 1) * 512;
        header = (struct tar_header *)address;
    }

    if (!init_found) {
        panic("No init script found!\n");
    }
}