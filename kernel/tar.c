#include <kernel/tar.h>
#include <write/write.h>

// Convert octal ASCII string to unsigned integer
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

void tar_parse(uint32_t address) {
    struct tar_header *header = (struct tar_header *)address;

    k_print("Unpacking initramfs...\n");

    while (header->name[0] != '\0') {
        // Print found file name
        k_print("Found file: ");
        k_print(header->name);
        k_print("\n");

        uint32_t size = tar_to_int(header->size);
        
        // Data is located right after the 512-byte header block
        uint32_t file_data_address = address + 512;

        // Optional: If the file is a text file, you can safely print its content directly
        // k_print((const char *)file_data_address);

        // Jump to the next header: current address + header (512) + file size (padded to 512 bytes)
        uint32_t blocks = (size + 512 - 1) / 512;
        address += (blocks + 1) * 512;
        header = (struct tar_header *)address;
    }
}