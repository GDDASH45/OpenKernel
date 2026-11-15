#include <stdint.h>
#include <stddef.h>
#include <kernel/gdt.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_pointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct tss_entry {
    uint32_t previous;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct gdt_pointer gdtr;
static struct tss_entry tss;

extern void gdt_load(const struct gdt_pointer *pointer);
extern uint8_t stack_top;

static void gdt_set_entry(uint32_t index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity)
{
    gdt[index].base_low = (uint16_t)(base & 0xFFFFu);
    gdt[index].base_middle = (uint8_t)((base >> 16) & 0xFFu);
    gdt[index].base_high = (uint8_t)((base >> 24) & 0xFFu);
    gdt[index].limit_low = (uint16_t)(limit & 0xFFFFu);
    gdt[index].granularity =
        (uint8_t)(((limit >> 16) & 0x0Fu) | (granularity & 0xF0u));
    gdt[index].access = access;
}

void gdt_set_kernel_stack(uint32_t stack_top_address)
{
    tss.esp0 = stack_top_address;
    tss.ss0 = GDT_KERNEL_DATA;
}

void gdt_init(void)
{
    gdtr.limit = (uint16_t)(sizeof(gdt) - 1u);
    gdtr.base = (uint32_t)(uintptr_t)&gdt[0];

    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFu, 0x9Au, 0xCFu);
    gdt_set_entry(2, 0, 0xFFFFFu, 0x92u, 0xCFu);
    gdt_set_entry(3, 0, 0xFFFFFu, 0xFAu, 0xCFu);
    gdt_set_entry(4, 0, 0xFFFFFu, 0xF2u, 0xCFu);

    tss.ss0 = GDT_KERNEL_DATA;
    tss.esp0 = (uint32_t)(uintptr_t)&stack_top;
    tss.iomap_base = (uint16_t)sizeof(tss);

    gdt_set_entry(5, (uint32_t)(uintptr_t)&tss,
                  (uint32_t)sizeof(tss) - 1u, 0x89u, 0x00u);
    gdt_load(&gdtr);
}
