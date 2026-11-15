#ifndef MEMORY_VMM_H
#define MEMORY_VMM_H

#include <stdint.h>

#define VMM_PAGE_PRESENT 0x001u
#define VMM_PAGE_WRITE   0x002u
#define VMM_PAGE_USER    0x004u
#define VMM_PAGE_OWNED   0x200u

#define VMM_USER_BASE 0xC0000000u
#define VMM_USER_TOP  0xFFFFF000u

struct address_space {
    uint32_t *directory;
};

int vmm_init(void);
struct address_space *vmm_address_space_create(void);
void vmm_address_space_destroy(struct address_space *space);
int vmm_address_space_activate(struct address_space *space);
struct address_space *vmm_address_space_clone(struct address_space *space);
int vmm_copy_from_user(struct address_space *space, void *destination,
                       uint32_t source, uint32_t size);
int vmm_map_page(struct address_space *space, uint32_t virtual_address,
                 uint32_t physical_address, uint32_t flags);
int vmm_unmap_page(struct address_space *space, uint32_t virtual_address);
int vmm_protect_page(struct address_space *space, uint32_t virtual_address,
                     uint32_t flags);
uint32_t vmm_read_cr2(void);

#endif
