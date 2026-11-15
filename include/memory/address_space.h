#ifndef MEMORY_ADDRESS_SPACE_H
#define MEMORY_ADDRESS_SPACE_H

#include <memory/vmm.h>

static inline struct address_space *address_space_create(void)
{
    return vmm_address_space_create();
}

static inline void address_space_destroy(struct address_space *space)
{
    vmm_address_space_destroy(space);
}

static inline struct address_space *address_space_clone(struct address_space *space)
{
    return vmm_address_space_clone(space);
}

static inline int address_space_activate(struct address_space *space)
{
    return vmm_address_space_activate(space);
}

static inline int address_space_map(struct address_space *space,
                                    uint32_t virtual_address,
                                    uint32_t physical_address,
                                    uint32_t flags)
{
    return vmm_map_page(space, virtual_address, physical_address, flags);
}

static inline int address_space_unmap(struct address_space *space,
                                      uint32_t virtual_address)
{
    return vmm_unmap_page(space, virtual_address);
}

#endif
