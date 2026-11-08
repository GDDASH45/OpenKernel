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

static int load_kernel_elf(const uint8_t *image, uint32_t image_size,
                           uint32_t multiboot_address, uint32_t *entry) {
    const struct elf32_header *header;
    const struct elf32_program_header *headers[ELF_MAX_PROGRAM_HEADERS];
    uint32_t load_count = 0;
    int entry_is_executable = 0;

    if (image_size < sizeof(struct elf32_header)) {
        return -1;
    }
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
                header->program_header_size) {
        return -1;
    }

    for (uint32_t index = 0; index < header->program_header_count; index++) {
        const struct elf32_program_header *program =
            (const struct elf32_program_header *)(image +
                header->program_header_offset +
                index * header->program_header_size);
        uint32_t address;
        uint32_t end;

        if (program->type != PT_LOAD) {
            continue;
        }
        address = program->physical_address;
        if (program->memory_size < program->file_size ||
            program->offset > image_size ||
            program->file_size > image_size - program->offset ||
            address != program->virtual_address ||
            address < KERNEL_LOAD_MINIMUM ||
            address >= KERNEL_LOAD_LIMIT ||
            program->memory_size > KERNEL_LOAD_LIMIT - address) {
            return -1;
        }
        end = address + program->memory_size;
        for (uint32_t prior = 0; prior < load_count; prior++) {
            uint32_t prior_start = headers[prior]->physical_address;
            uint32_t prior_end = prior_start + headers[prior]->memory_size;
            if (address < prior_end && prior_start < end) {
                return -1;
            }
        }
        headers[load_count++] = program;
        if ((program->flags & PF_X) != 0 && header->entry >= address &&
            header->entry < end) {
            entry_is_executable = 1;
        }
    }
    if (load_count == 0 || !entry_is_executable) {
        return -1;
    }

    const struct multiboot_info *boot_info =
        (const struct multiboot_info *)(uintptr_t)multiboot_address;
    if (multiboot_address >= KERNEL_LOAD_MINIMUM &&
        multiboot_address < KERNEL_LOAD_LIMIT &&
        sizeof(*boot_info) <= KERNEL_LOAD_LIMIT - multiboot_address) {
        uint32_t boot_info_end = multiboot_address + sizeof(*boot_info);
        for (uint32_t index = 0; index < load_count; index++) {
            uint32_t start = headers[index]->physical_address;
            uint32_t end = start + headers[index]->memory_size;
            if (start < boot_info_end && multiboot_address < end) {
                return -1;
            }
        }
    }
    if ((boot_info->flags & (1u << 3)) != 0 &&
        boot_info->mods_count != 0 && boot_info->mods_addr != 0) {
        const struct multiboot_module *modules =
            (const struct multiboot_module *)(uintptr_t)boot_info->mods_addr;
        for (uint32_t module = 0; module < boot_info->mods_count; module++) {
            if (modules[module].mod_end < modules[module].mod_start) {
                return -1;
            }
            for (uint32_t index = 0; index < load_count; index++) {
                uint32_t start = headers[index]->physical_address;
                uint32_t end = start + headers[index]->memory_size;
                if (start < modules[module].mod_end &&
                    modules[module].mod_start < end) {
                    return -1;
                }
            }
        }
    }

    for (uint32_t index = 0; index < load_count; index++) {
        const struct elf32_program_header *program = headers[index];
        volatile uint8_t *destination =
            (volatile uint8_t *)(uintptr_t)program->physical_address;
        for (uint32_t byte = 0; byte < program->file_size; byte++) {
            destination[byte] = image[program->offset + byte];
        }
        for (uint32_t byte = program->file_size;
             byte < program->memory_size; byte++) {
            destination[byte] = 0;
        }
    }
    *entry = header->entry;
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
