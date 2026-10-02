#include <kernel/tar.h>
#include <kernel/script.h>
#include <kernel/exec.h>
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

static int str_match(const char *s1, const char *s2) {
    if (s1[0] == '.' && s1[1] == '/') s1 += 2;
    if (s2[0] == '.' && s2[1] == '/') s2 += 2;

    while (*s1 && (*s1 == *s2)) { 
        s1++; 
        s2++; 
    }
    return *s1 == *s2;
}

static int is_root_init(const char *name) {
    if (name[0] == '.' && name[1] == '/') {
        name += 2;
    }
    return name[0] == 'i' && name[1] == 'n' && name[2] == 'i' &&
           name[3] == 't' && name[4] == '\0';
}

static int is_text_script(const char *data, uint32_t size) {
    for (uint32_t index = 0; index < size; index++) {
        unsigned char character = (unsigned char)data[index];
        if (character != '\n' && character != '\r' && character != '\t' &&
            (character < 32 || character > 126)) {
            return 0;
        }
    }
    return 1;
}

const char* tar_get_file(uint32_t address, const char *target_name, uint32_t *out_size)
{
    struct tar_header *header = (struct tar_header *)address;

    // Validate magic signature before traversal
    if (header->magic[0] != 'u' || header->magic[1] != 's' || 
        header->magic[2] != 't' || header->magic[3] != 'a' || 
        header->magic[4] != 'r') {
        panic("Invalid or unrecognized TAR archive format in tar_get_file!");
    }

    while (header->name[0] != '\0')
    {
        uint32_t size = tar_to_int(header->size);

        if (str_match(header->name, target_name))
        {
            *out_size = size;
            return (const char *)(address + 512);
        }

        uint32_t blocks = (size + 512 - 1) / 512;
        address += (blocks + 1) * 512;
        header = (struct tar_header *)address;
    }
    return 0;
}

void tar_parse(uint32_t address) {
    uint32_t initrd_base = address;
    struct tar_header *header = (struct tar_header *)address;

    // Verify USTAR magic bytes to prevent parsing invalid/malicious memory structures
    if (header->magic[0] != 'u' || header->magic[1] != 's' || 
        header->magic[2] != 't' || header->magic[3] != 'a' || 
        header->magic[4] != 'r') {
        panic("Invalid or unrecognized TAR archive format!");
    }

    const char *init_data = 0;
    uint32_t init_size = 0;
    int found_boot = 0;
    int found_root = 0;
    int found_device = 0;

    while (header->name[0] != '\0') {
        uint32_t size = tar_to_int(header->size);
        uint32_t file_data_address = address + 512;

        const char *name = header->name;
        if (name[0] == '.' && name[1] == '/') name += 2;

        if ((name[0] == 'b' && name[1] == 'o' && name[2] == 'o' && name[3] == 't') && 
            (name[4] == '/' || name[4] == '\0')) {
            found_boot = 1;
        }
        if ((name[0] == 'r' && name[1] == 'o' && name[2] == 'o' && name[3] == 't') && 
            (name[4] == '/' || name[4] == '\0')) {
            found_root = 1;
        }
        if ((name[0] == 'd' && name[1] == 'e' && name[2] == 'v' && name[3] == 'i' && name[4] == 'c' && name[5] == 'e') && 
            (name[6] == '/' || name[6] == '\0')) {
            found_device = 1;
        }

        if (is_root_init(header->name)) {
            init_data = (const char *)file_data_address;
            init_size = size;
        }

        uint32_t blocks = (size + 512 - 1) / 512;
        address += (blocks + 1) * 512;
        header = (struct tar_header *)address;
    }

    if (found_boot) k_print("[VFS] Detected /boot directory\n");
    if (found_root) k_print("[VFS] Detected /root directory\n");
    if (found_device) k_print("[VFS] Detected /device directory\n");

    if (!found_boot) panic("/boot/ not found!");
    if (!found_root) panic("/root/ not found!");
    if (!found_device) panic("/device/ not found!");

    if (init_data == 0) {
        panic("/init not found in initramfs!\n");
    }

    k_print("[INIT] Attempting to execute /init\n");
    if (is_text_script(init_data, init_size)) {
        if (script_run(init_data, init_size, initrd_base) != 0) {
            panic("Failed to run /init script!");
        }
    } else {
        if (run_binary(initrd_base, "init") != 0) {
            panic("Failed to execute /init binary!");
        }
    }
}