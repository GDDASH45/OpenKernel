#include <stdint.h>
#include <stddef.h>
#include <memory/page.h>
#include <memory/vmm.h>

#define PAGE_DIRECTORY_ENTRIES 1024u
#define PAGE_TABLE_ENTRIES 1024u
#define PAGE_LARGE 0x080u
#define PAGE_ADDRESS 0xFFFFF000u
#define LARGE_PAGE_ADDRESS 0xFFC00000u
#define PAGE_FLAGS 0x003u
#define PAGE_USER_BIT 0x004u
#define PAGE_OWNED_BIT 0x200u

static uint32_t *kernel_directory;
static uint32_t *active_directory;

static void zero_page(void *page)
{
    uint32_t *words = (uint32_t *)page;

    for (uint32_t index = 0; index < PAGE_TABLE_ENTRIES; index++) {
        words[index] = 0;
    }
}

static void copy_page(void *destination, const void *source)
{
    uint32_t *dst = (uint32_t *)destination;
    const uint32_t *src = (const uint32_t *)source;

    for (uint32_t index = 0; index < PAGE_TABLE_ENTRIES; index++) {
        dst[index] = src[index];
    }
}

static uint32_t *page_table_for(struct address_space *space,
                                uint32_t virtual_address, int create)
{
    uint32_t directory_index = virtual_address >> 22;
    uint32_t entry = space->directory[directory_index];
    uint32_t *table;

    if ((entry & VMM_PAGE_PRESENT) == 0) {
        if (!create) {
            return 0;
        }

        table = (uint32_t *)memory_alloc_page();
        if (table == 0) {
            return 0;
        }
        zero_page(table);
        space->directory[directory_index] =
            (uint32_t)(uintptr_t)table |
            VMM_PAGE_PRESENT | VMM_PAGE_WRITE | VMM_PAGE_USER;
        return table;
    }

    if ((entry & PAGE_LARGE) != 0) {
        uint32_t *new_table;

        if (!create) {
            return 0;
        }

        new_table = (uint32_t *)memory_alloc_page();
        if (new_table == 0) {
            return 0;
        }

        uint32_t base = entry & LARGE_PAGE_ADDRESS;
        for (uint32_t index = 0; index < PAGE_TABLE_ENTRIES; index++) {
            new_table[index] = (base + index * MEMORY_PAGE_SIZE) |
                               VMM_PAGE_PRESENT | VMM_PAGE_WRITE;
        }

        space->directory[directory_index] =
            (uint32_t)(uintptr_t)new_table |
            VMM_PAGE_PRESENT | VMM_PAGE_WRITE | VMM_PAGE_USER;
        return new_table;
    }

    if ((entry & PAGE_USER_BIT) == 0) {
        space->directory[directory_index] |= PAGE_USER_BIT;
    }

    return (uint32_t *)(uintptr_t)(entry & PAGE_ADDRESS);
}

int vmm_init(void)
{
    kernel_directory = (uint32_t *)memory_kernel_page_directory();
    if (kernel_directory == 0) {
        return -1;
    }

    active_directory = kernel_directory;
    return 0;
}

struct address_space *vmm_address_space_create(void)
{
    struct address_space *space;
    uint32_t *directory;

    if (kernel_directory == 0) {
        return 0;
    }

    space = (struct address_space *)memory_alloc_page();
    if (space == 0) {
        return 0;
    }

    directory = (uint32_t *)memory_alloc_page();
    if (directory == 0) {
        memory_free_page(space);
        return 0;
    }

    copy_page(directory, kernel_directory);
    space->directory = directory;
    return space;
}

void vmm_address_space_destroy(struct address_space *space)
{
    if (space == 0 || space->directory == 0 ||
        space->directory == active_directory) {
        return;
    }

    for (uint32_t directory_index = 0;
         directory_index < PAGE_DIRECTORY_ENTRIES;
         directory_index++) {
        uint32_t directory_entry = space->directory[directory_index];

        if ((directory_entry & VMM_PAGE_PRESENT) == 0 ||
            (directory_entry & PAGE_LARGE) != 0) {
            continue;
        }

        uint32_t *table =
            (uint32_t *)(uintptr_t)(directory_entry & PAGE_ADDRESS);

        for (uint32_t table_index = 0;
             table_index < PAGE_TABLE_ENTRIES;
             table_index++) {
            uint32_t page = table[table_index];

            if ((page & (VMM_PAGE_PRESENT | PAGE_OWNED_BIT)) ==
                (VMM_PAGE_PRESENT | PAGE_OWNED_BIT)) {
                memory_free_page((void *)(uintptr_t)(page & PAGE_ADDRESS));
            }
        }

        memory_free_page(table);
    }

    memory_free_page(space->directory);
    memory_free_page(space);
}

struct address_space *vmm_address_space_clone(struct address_space *source)
{
    struct address_space *destination;

    if (source == 0 || source->directory == 0 ||
        source->directory != active_directory) {
        return 0;
    }

    destination = vmm_address_space_create();
    if (destination == 0) {
        return 0;
    }

    for (uint32_t directory_index = VMM_USER_BASE >> 22;
         directory_index < PAGE_DIRECTORY_ENTRIES;
         directory_index++) {
        uint32_t directory_entry = source->directory[directory_index];
        uint32_t *source_table;

        if ((directory_entry & VMM_PAGE_PRESENT) == 0 ||
            (directory_entry & PAGE_USER_BIT) == 0) {
            continue;
        }
        if ((directory_entry & PAGE_LARGE) != 0) {
            vmm_address_space_destroy(destination);
            return 0;
        }

        source_table = (uint32_t *)(uintptr_t)(directory_entry & PAGE_ADDRESS);
        for (uint32_t table_index = 0; table_index < PAGE_TABLE_ENTRIES;
             table_index++) {
            uint32_t page_entry = source_table[table_index];
            uint32_t virtual_address =
                (directory_index << 22) | (table_index << 12);
            void *page;
            uint32_t flags;

            if ((page_entry & (VMM_PAGE_PRESENT | PAGE_USER_BIT)) !=
                (VMM_PAGE_PRESENT | PAGE_USER_BIT)) {
                continue;
            }

            page = memory_alloc_page();
            if (page == 0) {
                vmm_address_space_destroy(destination);
                return 0;
            }
            copy_page(page, (const void *)(uintptr_t)virtual_address);

            flags = VMM_PAGE_USER | VMM_PAGE_OWNED;
            if (page_entry & VMM_PAGE_WRITE) {
                flags |= VMM_PAGE_WRITE;
            }
            if (vmm_map_page(destination, virtual_address,
                             (uint32_t)(uintptr_t)page, flags) != 0) {
                memory_free_page(page);
                vmm_address_space_destroy(destination);
                return 0;
            }
        }
    }
    return destination;
}

int vmm_address_space_activate(struct address_space *space)
{
    uint32_t *directory = space != 0 ? space->directory : kernel_directory;
    uint32_t address;

    if (directory == 0) {
        return -1;
    }

    address = memory_kernel_virtual_to_physical(directory);
    if (directory != active_directory) {
        __asm__ volatile ("mov %0, %%cr3" : : "r"(address) : "memory");
        active_directory = directory;
    }

    return 0;
}

int vmm_copy_from_user(struct address_space *space, void *destination,
                       uint32_t source, uint32_t size)
{
    uint8_t *output = (uint8_t *)destination;

    if (size == 0) {
        return 0;
    }
    if (space == 0 || space->directory == 0 || destination == 0 ||
        source < VMM_USER_BASE ||
        source > UINT32_MAX - (size - 1u)) {
        return -1;
    }

    while (size != 0) {
        uint32_t directory_entry = space->directory[source >> 22];
        uint32_t page_offset = source & (MEMORY_PAGE_SIZE - 1u);
        uint32_t chunk = MEMORY_PAGE_SIZE - page_offset;
        uint32_t *table;
        uint32_t page_entry;

        if ((directory_entry & (VMM_PAGE_PRESENT | PAGE_USER_BIT)) !=
            (VMM_PAGE_PRESENT | PAGE_USER_BIT) ||
            (directory_entry & PAGE_LARGE) != 0) {
            return -1;
        }
        table = (uint32_t *)(uintptr_t)(directory_entry & PAGE_ADDRESS);
        page_entry = table[(source >> 12) & 0x3FFu];
        if ((page_entry & (VMM_PAGE_PRESENT | PAGE_USER_BIT)) !=
            (VMM_PAGE_PRESENT | PAGE_USER_BIT)) {
            return -1;
        }
        if (chunk > size) {
            chunk = size;
        }

        const uint8_t *input = (const uint8_t *)(uintptr_t)source;
        for (uint32_t index = 0; index < chunk; index++) {
            output[index] = input[index];
        }
        output += chunk;
        source += chunk;
        size -= chunk;
    }
    return 0;
}

int vmm_map_page(struct address_space *space, uint32_t virtual_address,
                 uint32_t physical_address, uint32_t flags)
{
    uint32_t *table;
    uint32_t index;

    if (space == 0 || space->directory == 0 ||
        (virtual_address & (MEMORY_PAGE_SIZE - 1u)) != 0 ||
        (physical_address & (MEMORY_PAGE_SIZE - 1u)) != 0 ||
        virtual_address < VMM_USER_BASE ||
        physical_address >= VMM_USER_BASE ||
        (flags & ~(VMM_PAGE_WRITE | VMM_PAGE_USER | VMM_PAGE_OWNED)) != 0) {
        return -1;
    }

    table = page_table_for(space, virtual_address, 1);
    if (table == 0) {
        return -1;
    }

    index = (virtual_address >> 12) & 0x3FFu;
    if ((table[index] & VMM_PAGE_PRESENT) != 0) {
        return -1;
    }

    table[index] = physical_address | VMM_PAGE_PRESENT |
                   (flags & (VMM_PAGE_WRITE | VMM_PAGE_USER |
                             VMM_PAGE_OWNED));

    if (space->directory == active_directory) {
        __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    }

    return 0;
}

int vmm_unmap_page(struct address_space *space, uint32_t virtual_address)
{
    uint32_t *table;
    uint32_t index;
    uint32_t entry;

    if (space == 0 || space->directory == 0 ||
        (virtual_address & (MEMORY_PAGE_SIZE - 1u)) != 0 ||
        virtual_address < VMM_USER_BASE) {
        return -1;
    }

    table = page_table_for(space, virtual_address, 0);
    if (table == 0) {
        return -1;
    }

    index = (virtual_address >> 12) & 0x3FFu;
    entry = table[index];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return -1;
    }

    table[index] = 0;
    if (space->directory == active_directory) {
        __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    }

    if ((entry & PAGE_OWNED_BIT) != 0) {
        return memory_free_page((void *)(uintptr_t)(entry & PAGE_ADDRESS));
    }

    return 0;
}

int vmm_protect_page(struct address_space *space, uint32_t virtual_address,
                     uint32_t flags)
{
    uint32_t *table;
    uint32_t index;
    uint32_t entry;

    if (space == 0 || space->directory == 0 ||
        (virtual_address & (MEMORY_PAGE_SIZE - 1u)) != 0 ||
        virtual_address < VMM_USER_BASE ||
        (flags & ~(VMM_PAGE_WRITE | VMM_PAGE_USER)) != 0) {
        return -1;
    }

    table = page_table_for(space, virtual_address, 0);
    if (table == 0) {
        return -1;
    }

    index = (virtual_address >> 12) & 0x3FFu;
    entry = table[index];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return -1;
    }

    table[index] = (entry & PAGE_ADDRESS) | VMM_PAGE_PRESENT |
                   (entry & PAGE_OWNED_BIT) |
                   (flags & (VMM_PAGE_WRITE | VMM_PAGE_USER));

    if (space->directory == active_directory) {
        __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    }

    return 0;
}

uint32_t vmm_read_cr2(void)
{
    uint32_t address;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(address));
    return address;
}
