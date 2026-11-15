#include <stdint.h>
#include <kernel/multiboot.h>

#define ELF_BUFFER_CAPACITY (2u * 1024u * 1024u)
#define KERNEL_LOAD_MINIMUM 0x00400000u
#define KERNEL_LOAD_LIMIT   0xC0000000u
#define ELF_MAX_PROGRAM_HEADERS 64u
#define PT_LOAD 1u
#define PF_X 1u

extern const uint8_t _binary_openkernel_elf_gz_start[];
extern const uint8_t _binary_openkernel_elf_gz_end[];
extern void kernel_loader_jump(uint32_t entry, uint32_t magic,
                               uint32_t multiboot_address, uint32_t oem)
    __attribute__((noreturn));

static uint8_t elf_buffer[ELF_BUFFER_CAPACITY];

struct gzip_bits {
    const uint8_t *data;
    uint32_t size;
    uint32_t bit_position;
};

struct huffman_tree {
    uint16_t count[16];
    uint16_t first_code[16];
    uint16_t first_symbol[16];
    uint16_t symbols[288];
};

struct elf32_header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t program_header_offset;
    uint32_t section_header_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_size;
    uint16_t program_header_count;
    uint16_t section_header_size;
    uint16_t section_header_count;
    uint16_t section_string_index;
} __attribute__((packed));

struct elf32_program_header {
    uint32_t type;
    uint32_t offset;
    uint32_t virtual_address;
    uint32_t physical_address;
    uint32_t file_size;
    uint32_t memory_size;
    uint32_t flags;
    uint32_t alignment;
} __attribute__((packed));

static __attribute__((noreturn)) void loader_halt(const char *message) {
    volatile uint16_t *video = (volatile uint16_t *)0xB8000;
    uint32_t index = 0;

    while (message[index] != '\0' && index < 80u) {
        video[index] = (uint16_t)(0x4F00u | (uint8_t)message[index]);
        index++;
    }
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static int read_bits(struct gzip_bits *bits, uint32_t count,
                     uint32_t *value) {
    uint32_t result = 0;

    if (count > 16u || bits->bit_position > bits->size * 8u ||
        count > bits->size * 8u - bits->bit_position) {
        return -1;
    }
    for (uint32_t index = 0; index < count; index++) {
        uint32_t position = bits->bit_position++;
        result |= ((bits->data[position >> 3] >> (position & 7u)) & 1u)
                  << index;
    }
    *value = result;
    return 0;
}

static int align_bits(struct gzip_bits *bits) {
    uint32_t remainder = bits->bit_position & 7u;
    if (remainder != 0) {
        bits->bit_position += 8u - remainder;
    }
    return bits->bit_position <= bits->size * 8u ? 0 : -1;
}

static int build_tree(struct huffman_tree *tree, const uint8_t *lengths,
                      uint32_t symbol_count) {
    uint16_t offsets[16];
    uint32_t code = 0;
    uint32_t symbol_offset = 0;

    for (uint32_t index = 0; index < 16; index++) {
        tree->count[index] = 0;
    }
    for (uint32_t symbol = 0; symbol < symbol_count; symbol++) {
        if (lengths[symbol] > 15u) {
            return -1;
        }
        if (lengths[symbol] != 0) {
            tree->count[lengths[symbol]]++;
        }
    }
    tree->count[0] = 0;
    for (uint32_t length = 1; length <= 15; length++) {
        code = (code + tree->count[length - 1]) << 1;
        if (code + tree->count[length] > (1u << length)) {
            return -1;
        }
        tree->first_code[length] = (uint16_t)code;
        tree->first_symbol[length] = (uint16_t)symbol_offset;
        symbol_offset += tree->count[length];
        offsets[length] = tree->first_symbol[length];
    }
    for (uint32_t symbol = 0; symbol < symbol_count; symbol++) {
        uint8_t length = lengths[symbol];
        if (length != 0) {
            tree->symbols[offsets[length]++] = (uint16_t)symbol;
        }
    }
    return 0;
}

static int decode_symbol(struct gzip_bits *bits,
                         const struct huffman_tree *tree,
                         uint32_t *symbol) {
    uint32_t code = 0;

    for (uint32_t length = 1; length <= 15; length++) {
        uint32_t bit;
        if (read_bits(bits, 1, &bit) != 0) {
            return -1;
        }
        code = (code << 1) | bit;
        if (code >= tree->first_code[length] &&
            code - tree->first_code[length] < tree->count[length]) {
            *symbol = tree->symbols[tree->first_symbol[length] +
                                    code - tree->first_code[length]];
            return 0;
        }
    }
    return -1;
}

static int inflate_codes(struct gzip_bits *bits,
                         const struct huffman_tree *literal_tree,
                         const struct huffman_tree *distance_tree,
                         uint8_t *output, uint32_t capacity,
                         uint32_t *output_size) {
    static const uint16_t length_base[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
    };
    static const uint8_t length_extra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3,
        3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
    };
    static const uint16_t distance_base[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
        193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
        6145, 8193, 12289, 16385, 24577
    };
    static const uint8_t distance_extra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
        8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };

    for (;;) {
        uint32_t symbol;
        if (decode_symbol(bits, literal_tree, &symbol) != 0) {
            return -1;
        }
        if (symbol < 256u) {
            if (*output_size == capacity) {
                return -1;
            }
            output[(*output_size)++] = (uint8_t)symbol;
        } else if (symbol == 256u) {
            return 0;
        } else {
            uint32_t length_index;
            uint32_t extra;
            uint32_t length;
            uint32_t distance_symbol;
            uint32_t distance;

            if (symbol < 257u || symbol > 285u) {
                return -1;
            }
            length_index = symbol - 257u;
            if (read_bits(bits, length_extra[length_index], &extra) != 0) {
                return -1;
            }
            length = length_base[length_index] + extra;
            if (decode_symbol(bits, distance_tree, &distance_symbol) != 0 ||
                distance_symbol >= 30u ||
                read_bits(bits, distance_extra[distance_symbol], &extra) != 0) {
                return -1;
            }
            distance = distance_base[distance_symbol] + extra;
            if (distance > *output_size ||
                length > capacity - *output_size) {
                return -1;
            }
            for (uint32_t index = 0; index < length; index++) {
                output[*output_size] =
                    output[*output_size - distance];
                (*output_size)++;
            }
        }
    }
}

static int inflate_dynamic_block(struct gzip_bits *bits, uint8_t *output,
                                 uint32_t capacity, uint32_t *output_size) {
    static const uint8_t code_length_order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };
    uint8_t code_lengths[19] = {0};
    uint8_t lengths[286 + 32];
    struct huffman_tree code_tree;
    struct huffman_tree literal_tree;
    struct huffman_tree distance_tree;
    uint32_t value;
    uint32_t literal_count;
    uint32_t distance_count;
    uint32_t code_count;
    uint32_t index = 0;

    if (read_bits(bits, 5, &value) != 0) {
        return -1;
    }
    literal_count = value + 257u;
    if (literal_count > 286u) {
        return -1;
    }
    if (read_bits(bits, 5, &value) != 0) {
        return -1;
    }
    distance_count = value + 1u;
    if (read_bits(bits, 4, &value) != 0) {
        return -1;
    }
    code_count = value + 4u;

    for (uint32_t item = 0; item < code_count; item++) {
        if (read_bits(bits, 3, &value) != 0) {
            return -1;
        }
        code_lengths[code_length_order[item]] = (uint8_t)value;
    }
    if (build_tree(&code_tree, code_lengths, 19) != 0) {
        return -1;
    }

    while (index < literal_count + distance_count) {
        uint32_t symbol;
        if (decode_symbol(bits, &code_tree, &symbol) != 0) {
            return -1;
        }
        if (symbol <= 15u) {
            lengths[index++] = (uint8_t)symbol;
        } else {
            uint32_t repeat;
            uint8_t repeated_value = 0;
            if (symbol == 16u) {
                if (index == 0 || read_bits(bits, 2, &value) != 0) {
                    return -1;
                }
                repeat = value + 3u;
                repeated_value = lengths[index - 1];
            } else if (symbol == 17u) {
                if (read_bits(bits, 3, &value) != 0) {
                    return -1;
                }
                repeat = value + 3u;
            } else if (symbol == 18u) {
                if (read_bits(bits, 7, &value) != 0) {
                    return -1;
                }
                repeat = value + 11u;
            } else {
                return -1;
            }
            if (repeat > literal_count + distance_count - index) {
                return -1;
            }
            while (repeat-- != 0) {
                lengths[index++] = repeated_value;
            }
        }
    }
    if (lengths[256] == 0 ||
        build_tree(&literal_tree, lengths, literal_count) != 0 ||
        build_tree(&distance_tree, lengths + literal_count,
                   distance_count) != 0) {
        return -1;
    }
    return inflate_codes(bits, &literal_tree, &distance_tree, output,
                         capacity, output_size);
}

static int inflate_deflate(const uint8_t *input, uint32_t input_size,
                           uint8_t *output, uint32_t capacity,
                           uint32_t *output_size, uint32_t *consumed) {
    struct gzip_bits bits = {input, input_size, 0};
    uint32_t final_block = 0;

    *output_size = 0;
    while (!final_block) {
        uint32_t block_type;
        uint32_t value;
        struct huffman_tree literal_tree;
        struct huffman_tree distance_tree;

        if (read_bits(&bits, 1, &final_block) != 0 ||
            read_bits(&bits, 2, &block_type) != 0) {
            return -1;
        }
        if (block_type == 0) {
            uint32_t length;
            uint32_t complement;
            if (align_bits(&bits) != 0 ||
                read_bits(&bits, 16, &length) != 0 ||
                read_bits(&bits, 16, &complement) != 0 ||
                ((length ^ 0xFFFFu) & 0xFFFFu) != complement ||
                length > capacity - *output_size) {
                return -1;
            }
            for (uint32_t index = 0; index < length; index++) {
                if (read_bits(&bits, 8, &value) != 0) {
                    return -1;
                }
                output[(*output_size)++] = (uint8_t)value;
            }
        } else if (block_type == 1) {
            uint8_t literal_lengths[288];
            uint8_t distance_lengths[32];
            for (uint32_t symbol = 0; symbol <= 287u; symbol++) {
                literal_lengths[symbol] =
                    symbol <= 143u ? 8u :
                    symbol <= 255u ? 9u :
                    symbol <= 279u ? 7u : 8u;
            }
            for (uint32_t symbol = 0; symbol < 32u; symbol++) {
                distance_lengths[symbol] = 5u;
            }
            if (build_tree(&literal_tree, literal_lengths, 288) != 0 ||
                build_tree(&distance_tree, distance_lengths, 32) != 0 ||
                inflate_codes(&bits, &literal_tree, &distance_tree, output,
                              capacity, output_size) != 0) {
                return -1;
            }
        } else if (block_type == 2) {
            if (inflate_dynamic_block(&bits, output, capacity,
                                      output_size) != 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    if (align_bits(&bits) != 0) {
        return -1;
    }
    *consumed = bits.bit_position >> 3;
    return 0;
}

static uint32_t crc32(const uint8_t *data, uint32_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t index = 0; index < size; index++) {
        crc ^= data[index];
        for (uint32_t bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static uint32_t read_le32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int inflate_gzip(const uint8_t *input, uint32_t input_size,
                        uint8_t *output, uint32_t capacity,
                        uint32_t *output_size) {
    uint32_t header_size = 10;
    uint8_t flags;
    uint32_t deflate_size;
    uint32_t consumed;
    uint32_t inflated_size;

    if (input_size < 18u || input[0] != 0x1Fu || input[1] != 0x8Bu ||
        input[2] != 8u || (input[3] & 0xE0u) != 0) {
        return -1;
    }
    flags = input[3];
    if ((flags & 0x04u) != 0) {
        if (header_size + 2u > input_size - 8u) {
            return -1;
        }
        uint32_t extra_size = (uint32_t)input[header_size] |
                              ((uint32_t)input[header_size + 1] << 8);
        header_size += 2u;
        if (extra_size > input_size - 8u - header_size) {
            return -1;
        }
        header_size += extra_size;
    }
    for (uint32_t flag = 0x08u; flag <= 0x10u; flag <<= 1) {
        if ((flags & flag) != 0) {
            while (header_size < input_size - 8u &&
                   input[header_size++] != 0) {
            }
            if (header_size == input_size - 8u &&
                input[header_size - 1] != 0) {
                return -1;
            }
        }
    }
    if ((flags & 0x02u) != 0) {
        if (header_size + 2u > input_size - 8u) {
            return -1;
        }
        header_size += 2u;
    }
    if (header_size > input_size - 8u) {
        return -1;
    }

    deflate_size = input_size - header_size - 8u;
    if (inflate_deflate(input + header_size, deflate_size, output, capacity,
                        &inflated_size, &consumed) != 0 ||
        consumed > deflate_size ||
        header_size + consumed + 8u != input_size ||
        read_le32(input + header_size + consumed) != crc32(output, inflated_size) ||
        read_le32(input + header_size + consumed + 4u) != inflated_size) {
        return -1;
    }
    *output_size = inflated_size;
    return 0;
}


#define ELF_MAX_SECTION_HEADERS 4096u
#define ELF_SHF_ALLOC 0x2u
#define ELF_SHT_SYMTAB 2u
#define ELF_SHT_REL 9u
#define ELF_SHN_UNDEF 0u
#define ELF_R_386_NONE 0u
#define ELF_R_386_32 1u
#define ELF_R_386_PC32 2u
#define ELF_R_386_PLT32 4u
#define KASLR_MINIMUM 0x40000000u
#define KASLR_MAXIMUM 0xC0000000u
#define KASLR_GRANULARITY 0x00400000u
#define PAGE_DIRECTORY_ENTRIES 1024u
#define PAGE_SIZE_4M 0x00400000u
#define PAGE_FLAGS_4M 0x083u

struct elf32_section_header {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t address;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t alignment;
    uint32_t entry_size;
} __attribute__((packed));

struct elf32_symbol {
    uint32_t name;
    uint32_t value;
    uint32_t size;
    uint8_t info;
    uint8_t other;
    uint16_t section_index;
} __attribute__((packed));

struct elf32_rel {
    uint32_t offset;
    uint32_t info;
} __attribute__((packed));

struct loader_mmap_entry {
    uint32_t size;
    uint64_t address;
    uint64_t length;
    uint32_t type;
} __attribute__((packed));

static uint32_t kaslr_random_state;
static uint32_t kaslr_page_directory[PAGE_DIRECTORY_ENTRIES]
    __attribute__((aligned(4096)));

static uint32_t kaslr_random(void) {
    uint32_t value = kaslr_random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    kaslr_random_state = value;
    return value;
}

static int range_overlaps(uint32_t first, uint32_t end,
                          uint32_t other_first, uint32_t other_end) {
    return first < other_end && other_first < end;
}

/* The kernel's virtual alias replaces identity mappings in this window. */
static int kaslr_range_available(const struct multiboot_info *boot_info,
                                 uint32_t first, uint32_t end) {
    if (first < KASLR_MINIMUM || end <= first || end > KASLR_MAXIMUM)
        return 0;

    /* Only overlay addresses whose physical identity aliases are safe. */
    if ((boot_info->flags & (1u << 6)) == 0 ||
        boot_info->mmap_addr == 0 || boot_info->mmap_length < 24u ||
        boot_info->mmap_addr > 0xFFFFFFFFu - boot_info->mmap_length)
        return 0;
    {
        uint32_t offset = 0;
        while (offset + sizeof(uint32_t) <= boot_info->mmap_length) {
            const struct loader_mmap_entry *entry =
                (const struct loader_mmap_entry *)(uintptr_t)
                    (boot_info->mmap_addr + offset);
            uint64_t region_end;
            if (entry->size < 20u ||
                entry->size > boot_info->mmap_length - offset -
                              sizeof(uint32_t))
                return 0;
            region_end = entry->address + entry->length;
            if (region_end < entry->address)
                return 0;
            if (entry->type != 1u &&
                (uint64_t)first < region_end &&
                entry->address < (uint64_t)end)
                return 0;
            offset += entry->size + sizeof(uint32_t);
        }
        if (offset != boot_info->mmap_length)
            return 0;
    }

    if ((uint32_t)(uintptr_t)boot_info >
        0xFFFFFFFFu - (uint32_t)sizeof(*boot_info))
        return 0;
    if (range_overlaps(first, end, (uint32_t)(uintptr_t)boot_info,
                       (uint32_t)(uintptr_t)boot_info + sizeof(*boot_info)))
        return 0;

    if ((boot_info->flags & (1u << 6)) != 0 &&
        boot_info->mmap_addr != 0 && boot_info->mmap_length != 0) {
        if (boot_info->mmap_addr > 0xFFFFFFFFu - boot_info->mmap_length ||
            range_overlaps(first, end, boot_info->mmap_addr,
                           boot_info->mmap_addr + boot_info->mmap_length))
            return 0;
    }

    if ((boot_info->flags & (1u << 3)) != 0 &&
        boot_info->mods_count != 0 && boot_info->mods_addr != 0) {
        const struct multiboot_module *modules =
            (const struct multiboot_module *)(uintptr_t)boot_info->mods_addr;
        uint32_t module_table_size;

        if (boot_info->mods_count > 0xFFFFFFFFu / sizeof(*modules))
            return 0;
        module_table_size = boot_info->mods_count * sizeof(*modules);
        if (boot_info->mods_addr > 0xFFFFFFFFu - module_table_size ||
            range_overlaps(first, end, boot_info->mods_addr,
                           boot_info->mods_addr + module_table_size))
            return 0;
        for (uint32_t index = 0; index < boot_info->mods_count; index++) {
            if (modules[index].mod_end < modules[index].mod_start ||
                range_overlaps(first, end, modules[index].mod_start,
                               modules[index].mod_end))
                return 0;
        }
    }

    if ((boot_info->flags & (1u << 4)) != 0) {
        uint32_t symbols_size = boot_info->syms[0] + boot_info->syms[1];
        if (symbols_size < boot_info->syms[0] ||
            boot_info->syms[2] > 0xFFFFFFFFu - symbols_size ||
            range_overlaps(first, end, boot_info->syms[2],
                           boot_info->syms[2] + symbols_size))
            return 0;
    } else if ((boot_info->flags & (1u << 5)) != 0) {
        uint32_t symbols_size;
        if (boot_info->syms[1] != 0 &&
            boot_info->syms[0] > 0xFFFFFFFFu / boot_info->syms[1])
            return 0;
        symbols_size = boot_info->syms[0] * boot_info->syms[1];
        if (boot_info->syms[2] > 0xFFFFFFFFu - symbols_size ||
            range_overlaps(first, end, boot_info->syms[2],
                           boot_info->syms[2] + symbols_size))
            return 0;
    }

    if ((boot_info->flags & (1u << 2)) != 0 &&
        boot_info->cmdline != 0) {
        const char *command_line =
            (const char *)(uintptr_t)boot_info->cmdline;
        uint32_t length = 0;
        while (length < 4096u && command_line[length] != '\0')
            length++;
        if (length == 4096u ||
            boot_info->cmdline > 0xFFFFFFFFu - length - 1u ||
            range_overlaps(first, end, boot_info->cmdline,
                           boot_info->cmdline + length + 1u))
            return 0;
    }

    if ((boot_info->flags & (1u << 7)) != 0 &&
        boot_info->drives_addr != 0 && boot_info->drives_length != 0) {
        if (boot_info->drives_addr >
                0xFFFFFFFFu - boot_info->drives_length ||
            range_overlaps(first, end, boot_info->drives_addr,
                           boot_info->drives_addr + boot_info->drives_length))
            return 0;
    }

    if ((boot_info->flags & (1u << 9)) != 0 &&
        boot_info->boot_loader_name != 0) {
        const char *name =
            (const char *)(uintptr_t)boot_info->boot_loader_name;
        uint32_t length = 0;
        while (length < 256u && name[length] != '\0')
            length++;
        if (length == 256u ||
            boot_info->boot_loader_name > 0xFFFFFFFFu - length - 1u ||
            range_overlaps(first, end, boot_info->boot_loader_name,
                           boot_info->boot_loader_name + length + 1u))
            return 0;
    }

    if ((boot_info->flags & (1u << 8)) != 0 &&
        boot_info->config_table != 0) {
        if (boot_info->config_table > 0xFFFFFFFFu - 16u ||
            range_overlaps(first, end, boot_info->config_table,
                           boot_info->config_table + 16u))
            return 0;
    }

    if ((boot_info->flags & (1u << 10)) != 0 &&
        boot_info->apm_table != 0) {
        if (boot_info->apm_table > 0xFFFFFFFFu - 20u ||
            range_overlaps(first, end, boot_info->apm_table,
                           boot_info->apm_table + 20u))
            return 0;
    }

    if ((boot_info->flags & (1u << 11)) != 0) {
        if (boot_info->vbe_control_info > 0xFFFFFFFFu - 512u ||
            boot_info->vbe_mode_info > 0xFFFFFFFFu - 256u ||
            range_overlaps(first, end, boot_info->vbe_control_info,
                           boot_info->vbe_control_info + 512u) ||
            range_overlaps(first, end, boot_info->vbe_mode_info,
                           boot_info->vbe_mode_info + 256u))
            return 0;
    }

    if ((boot_info->flags & (1u << 12)) != 0) {
        uint64_t framebuffer_end = boot_info->framebuffer_addr +
            (uint64_t)boot_info->framebuffer_pitch *
            boot_info->framebuffer_height;
        if (framebuffer_end < boot_info->framebuffer_addr)
            return 0;
        if ((uint64_t)first < framebuffer_end &&
            boot_info->framebuffer_addr < (uint64_t)end)
            return 0;
        if (boot_info->framebuffer_type == 0 &&
            boot_info->framebuffer_palette_num_colors != 0) {
            uint32_t palette_size =
                (uint32_t)boot_info->framebuffer_palette_num_colors * 3u;
            if (boot_info->framebuffer_palette_addr >
                    0xFFFFFFFFu - palette_size ||
                range_overlaps(first, end,
                               boot_info->framebuffer_palette_addr,
                               boot_info->framebuffer_palette_addr +
                                   palette_size))
                return 0;
        }
    }
    return 1;
}

static int kaslr_is_disabled(const struct multiboot_info *boot_info) {
    const char *line;
    uint32_t index = 0;

    if ((boot_info->flags & (1u << 2)) == 0 || boot_info->cmdline == 0)
        return 0;
    line = (const char *)(uintptr_t)boot_info->cmdline;
    while (index < 4096u && line[index] != '\0') {
        uint32_t start;
        uint32_t length;
        while (index < 4096u &&
               (line[index] == ' ' || line[index] == '\t'))
            index++;
        start = index;
        while (index < 4096u && line[index] != '\0' &&
               line[index] != ' ' && line[index] != '\t')
            index++;
        length = index - start;
        if ((length == 7u &&
             line[start] == 'n' && line[start + 1] == 'o' &&
             line[start + 2] == 'k' && line[start + 3] == 'a' &&
             line[start + 4] == 's' && line[start + 5] == 'l' &&
             line[start + 6] == 'r') ||
            (length == 9u &&
             line[start] == 'k' && line[start + 1] == 'a' &&
             line[start + 2] == 's' && line[start + 3] == 'l' &&
             line[start + 4] == 'r' && line[start + 5] == '=' &&
             line[start + 6] == 'o' && line[start + 7] == 'f' &&
             line[start + 8] == 'f'))
            return 1;
    }
    return 0;
}

static uint32_t choose_kernel_slide(const struct multiboot_info *boot_info,
                                    uint32_t minimum, uint32_t maximum) {
    uint32_t span = maximum - minimum;
    uint32_t mapped_span =
        (span + KASLR_GRANULARITY - 1u) & ~(KASLR_GRANULARITY - 1u);
    uint32_t first_candidate;
    uint32_t slots;
    uint32_t seed;
    uint32_t tsc_low;
    uint32_t tsc_high;

    if (kaslr_is_disabled(boot_info) || span == 0 ||
        (minimum & (KASLR_GRANULARITY - 1u)) != 0 ||
        mapped_span < span ||
        mapped_span >= KASLR_MAXIMUM - KASLR_MINIMUM)
        return 0;

    __asm__ volatile ("rdtsc" : "=a"(tsc_low), "=d"(tsc_high));
    seed = tsc_low ^ (tsc_high * 0x9E3779B9u) ^
           (uint32_t)(uintptr_t)boot_info ^
           boot_info->mmap_length ^ boot_info->mods_count ^
           (uint32_t)(uintptr_t)&seed;
    if (seed == 0)
        seed = 0xA341316Cu;
    kaslr_random_state = seed;

    slots = (KASLR_MAXIMUM - KASLR_MINIMUM - mapped_span) /
            KASLR_GRANULARITY;
    if (slots == 0)
        return 0;
    first_candidate = kaslr_random() % slots;

    for (uint32_t attempt = 0; attempt < slots; attempt++) {
        uint32_t slot = (first_candidate + attempt) % slots;
        uint32_t candidate = KASLR_MINIMUM +
                             slot * KASLR_GRANULARITY;
        uint32_t end = candidate + mapped_span;
        if (kaslr_range_available(boot_info, candidate, end))
            return candidate - minimum;
    }
    return 0;
}

static volatile uint32_t *relocation_target(
    const struct elf32_program_header **headers, uint32_t load_count,
    uint32_t address) {
    for (uint32_t index = 0; index < load_count; index++) {
        uint32_t start = headers[index]->physical_address;
        uint32_t end = start + headers[index]->memory_size;
        if (address >= start && address <= end &&
            end - address >= sizeof(uint32_t))
            return (volatile uint32_t *)(uintptr_t)address;
    }
    return 0;
}

static int enable_kaslr_paging(uint32_t physical_base,
                               uint32_t image_size, uint32_t slide) {
    uint32_t virtual_base = physical_base + slide;
    uint32_t mapped_size =
        (image_size + PAGE_SIZE_4M - 1u) & ~(PAGE_SIZE_4M - 1u);
    uint32_t cr4;
    uint32_t cr0;

    if (slide == 0 || virtual_base < physical_base ||
        (physical_base & (PAGE_SIZE_4M - 1u)) != 0 ||
        (virtual_base & (PAGE_SIZE_4M - 1u)) != 0 ||
        virtual_base >= KASLR_MAXIMUM ||
        mapped_size < image_size ||
        mapped_size > KASLR_MAXIMUM - virtual_base)
        return -1;

    for (uint32_t index = 0; index < PAGE_DIRECTORY_ENTRIES; index++)
        kaslr_page_directory[index] = index * PAGE_SIZE_4M | PAGE_FLAGS_4M;

    for (uint32_t offset = 0; offset < image_size;
         offset += PAGE_SIZE_4M) {
        uint32_t virtual_address = virtual_base + offset;
        uint32_t physical_address = physical_base + offset;
        uint32_t directory_index = virtual_address >> 22;

        if (directory_index >= PAGE_DIRECTORY_ENTRIES ||
            physical_address >= KERNEL_LOAD_LIMIT)
            return -1;
        kaslr_page_directory[directory_index] =
            physical_address | PAGE_FLAGS_4M;
    }

    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x10u;
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");
    __asm__ volatile ("mov %0, %%cr3" : :
                      "r"((uint32_t)(uintptr_t)kaslr_page_directory) :
                      "memory");
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
    return 0;
}

/*
 * The kernel is linked at its physical load address. --emit-relocs preserves
 * static ELF relocation records so references can be redirected to the
 * randomized virtual alias while the image remains at its physical address.
 */
static int apply_kernel_relocations(
    const uint8_t *image, uint32_t image_size,
    const struct elf32_header *header,
    const struct elf32_program_header **headers, uint32_t load_count,
    uint32_t minimum, uint32_t maximum, uint32_t slide) {
    const struct elf32_section_header *sections;
    uint32_t section_bytes;

    if (slide == 0)
        return 0;
    if (header->section_header_count == 0 ||
        header->section_header_count > ELF_MAX_SECTION_HEADERS ||
        header->section_header_size != sizeof(*sections) ||
        header->section_header_offset > image_size)
        return -1;
    section_bytes = (uint32_t)header->section_header_count *
                    (uint32_t)sizeof(*sections);
    if (section_bytes > image_size - header->section_header_offset)
        return -1;
    sections = (const struct elf32_section_header *)(image +
                                      header->section_header_offset);

    for (uint32_t section_index = 0;
         section_index < header->section_header_count; section_index++) {
        const struct elf32_section_header *rel_section =
            &sections[section_index];
        const struct elf32_section_header *symbol_section;
        const struct elf32_symbol *symbols;
        uint32_t symbol_count;
        uint32_t relocation_count;

        if (rel_section->type != ELF_SHT_REL ||
            rel_section->info >= header->section_header_count ||
            (sections[rel_section->info].flags & ELF_SHF_ALLOC) == 0)
            continue;
        if (rel_section->link >= header->section_header_count ||
            rel_section->entry_size != sizeof(struct elf32_rel) ||
            rel_section->offset > image_size ||
            rel_section->size > image_size - rel_section->offset)
            return -1;

        symbol_section = &sections[rel_section->link];
        if (symbol_section->type != ELF_SHT_SYMTAB ||
            symbol_section->entry_size != sizeof(struct elf32_symbol) ||
            symbol_section->offset > image_size ||
            symbol_section->size > image_size - symbol_section->offset)
            return -1;
        if (symbol_section->size % sizeof(struct elf32_symbol) != 0)
            return -1;
        symbol_count = symbol_section->size /
                       sizeof(struct elf32_symbol);
        symbols = (const struct elf32_symbol *)(image +
                                                 symbol_section->offset);
        relocation_count = rel_section->size / sizeof(struct elf32_rel);
        if (rel_section->size % sizeof(struct elf32_rel) != 0)
            return -1;

        for (uint32_t relocation_index = 0;
             relocation_index < relocation_count; relocation_index++) {
            const struct elf32_rel *relocation =
                (const struct elf32_rel *)(image + rel_section->offset +
                    relocation_index * sizeof(struct elf32_rel));
            uint32_t symbol_index = relocation->info >> 8;
            uint32_t type = relocation->info & 0xFFu;
            const struct elf32_symbol *symbol;
            volatile uint32_t *target;
            uint32_t original_place = relocation->offset;
            uint32_t new_place = original_place + slide;
            uint32_t original_symbol;
            uint32_t new_symbol;
            uint32_t old_value;
            uint32_t addend;

            if (type == ELF_R_386_NONE)
                continue;
            if (type != ELF_R_386_32 && type != ELF_R_386_PC32 &&
                type != ELF_R_386_PLT32)
                return -1;
            if (symbol_index >= symbol_count ||
                original_place < minimum ||
                original_place > maximum - sizeof(uint32_t))
                return -1;
            target = relocation_target(headers, load_count, original_place);
            if (target == 0)
                return -1;
            symbol = &symbols[symbol_index];
            original_symbol = symbol->value;
            new_symbol = original_symbol;
            if (symbol->section_index != ELF_SHN_UNDEF &&
                original_symbol >= minimum && original_symbol <= maximum) {
                new_symbol += slide;
                if (new_symbol < original_symbol)
                    return -1;
            }
            old_value = *target;
            if (type == ELF_R_386_32) {
                addend = old_value - original_symbol;
                *target = new_symbol + addend;
            } else {
                addend = old_value - original_symbol + original_place;
                *target = new_symbol + addend - new_place;
            }
        }
    }
    return 0;
}

static int load_kernel_elf(const uint8_t *image, uint32_t image_size,
                           uint32_t multiboot_address, uint32_t *entry) {
    const struct elf32_header *header;
    const struct elf32_program_header *headers[ELF_MAX_PROGRAM_HEADERS];
    const struct multiboot_info *boot_info;
    uint32_t load_count = 0;
    uint32_t minimum = 0xFFFFFFFFu;
    uint32_t maximum = 0;
    uint32_t slide;
    int entry_is_executable = 0;

    if (image_size < sizeof(struct elf32_header))
        return -1;
    header = (const struct elf32_header *)image;
    if (header->ident[0] != 0x7Fu || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != 1u || header->ident[5] != 1u ||
        header->ident[6] != 1u || header->type != 2u ||
        header->machine != 3u || header->version != 1u ||
        header->header_size != sizeof(*header) ||
        header->program_header_size != sizeof(struct elf32_program_header) ||
        header->program_header_count == 0 ||
        header->program_header_count > ELF_MAX_PROGRAM_HEADERS ||
        header->program_header_offset > image_size ||
        header->program_header_count >
            (image_size - header->program_header_offset) /
                header->program_header_size)
        return -1;

    for (uint32_t index = 0; index < header->program_header_count; index++) {
        const struct elf32_program_header *program =
            (const struct elf32_program_header *)(image +
                header->program_header_offset +
                index * header->program_header_size);
        uint32_t address;
        uint32_t segment_end;

        if (program->type != PT_LOAD)
            continue;
        address = program->physical_address;
        if (program->memory_size < program->file_size ||
            program->offset > image_size ||
            program->file_size > image_size - program->offset ||
            address != program->virtual_address ||
            address < KERNEL_LOAD_MINIMUM ||
            address >= KERNEL_LOAD_LIMIT ||
            program->memory_size > KERNEL_LOAD_LIMIT - address ||
            program->memory_size == 0)
            return -1;
        segment_end = address + program->memory_size;
        for (uint32_t prior = 0; prior < load_count; prior++) {
            uint32_t prior_start = headers[prior]->physical_address;
            uint32_t prior_end = prior_start + headers[prior]->memory_size;
            if (address < prior_end && prior_start < segment_end)
                return -1;
        }
        headers[load_count++] = program;
        if (address < minimum)
            minimum = address;
        if (segment_end > maximum)
            maximum = segment_end;
        if ((program->flags & PF_X) != 0 &&
            header->entry >= address && header->entry < segment_end)
            entry_is_executable = 1;
    }
    if (load_count == 0 || !entry_is_executable || maximum <= minimum ||
        minimum != KERNEL_LOAD_MINIMUM)
        return -1;

    boot_info = (const struct multiboot_info *)(uintptr_t)multiboot_address;
    if (multiboot_address >= KERNEL_LOAD_MINIMUM &&
        multiboot_address < KERNEL_LOAD_LIMIT &&
        sizeof(*boot_info) <= KERNEL_LOAD_LIMIT - multiboot_address) {
        uint32_t boot_info_end = multiboot_address + sizeof(*boot_info);
        for (uint32_t index = 0; index < load_count; index++) {
            uint32_t segment_start = headers[index]->physical_address;
            uint32_t segment_end = segment_start +
                                   headers[index]->memory_size;
            if (segment_start < boot_info_end &&
                multiboot_address < segment_end)
                return -1;
        }
    }
    if ((boot_info->flags & (1u << 3)) != 0 &&
        boot_info->mods_count != 0 && boot_info->mods_addr != 0) {
        const struct multiboot_module *modules =
            (const struct multiboot_module *)(uintptr_t)boot_info->mods_addr;
        for (uint32_t module = 0; module < boot_info->mods_count; module++) {
            if (modules[module].mod_end < modules[module].mod_start)
                return -1;
            for (uint32_t index = 0; index < load_count; index++) {
                uint32_t segment_start = headers[index]->physical_address;
                uint32_t segment_end = segment_start +
                                       headers[index]->memory_size;
                if (segment_start < modules[module].mod_end &&
                    modules[module].mod_start < segment_end)
                    return -1;
            }
        }
    }

    slide = choose_kernel_slide(boot_info, minimum, maximum);
    for (uint32_t index = 0; index < load_count; index++) {
        const struct elf32_program_header *program = headers[index];
        uint32_t destination_address = program->physical_address;
        volatile uint8_t *destination;

        if (destination_address >= KERNEL_LOAD_LIMIT ||
            program->memory_size > KERNEL_LOAD_LIMIT - destination_address)
            return -1;
        destination = (volatile uint8_t *)(uintptr_t)destination_address;
        for (uint32_t byte = 0; byte < program->file_size; byte++)
            destination[byte] = image[program->offset + byte];
        for (uint32_t byte = program->file_size;
             byte < program->memory_size; byte++)
            destination[byte] = 0;
    }

    if (apply_kernel_relocations(image, image_size, header, headers,
                                 load_count, minimum, maximum, slide) != 0)
        return -1;
    if (header->entry + slide < header->entry ||
        header->entry + slide >= KASLR_MAXIMUM)
        return -1;
    if (slide != 0 &&
        enable_kaslr_paging(minimum, maximum - minimum, slide) != 0)
        return -1;
    *entry = header->entry + slide;
    return 0;
}

void kernel_loader_main(uint32_t magic, uint32_t multiboot_address,
                        uint32_t oem) {
    uint32_t payload_size =
        (uint32_t)(_binary_openkernel_elf_gz_end -
                   _binary_openkernel_elf_gz_start);
    uint32_t image_size;
    uint32_t entry;

    if (magic != 0x2BADB002u || multiboot_address == 0) {
        loader_halt("OpenKernel: invalid Multiboot handoff");
    }
    if (inflate_gzip(_binary_openkernel_elf_gz_start, payload_size,
                     elf_buffer, sizeof(elf_buffer), &image_size) != 0) {
        loader_halt("OpenKernel: gzip kernel decompression failed");
    }
    if (load_kernel_elf(elf_buffer, image_size, multiboot_address,
                        &entry) != 0) {
        loader_halt("OpenKernel: invalid compressed kernel ELF");
    }

    kernel_loader_jump(entry, magic, multiboot_address, oem);
}
