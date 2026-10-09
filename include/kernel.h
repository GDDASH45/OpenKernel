#ifndef KERNEL_H
#define KERNEL_H

#include <write/write.h>
#include <driver/sound.h>
#include <kvideo/fb.h>

struct kernel_panic_registers {
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t vector;
    uint32_t error_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
};

static inline void panic_print_hex(const char *name, uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";

    k_print(name);
    k_print("=0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        k_print_char(digits[(value >> shift) & 0x0F]);
    }
    k_print(" ");
}

static inline void panic_print_decimal(uint32_t value) {
    char digits[10];
    uint32_t count = 0;

    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);

    while (count != 0) {
        k_print_char(digits[--count]);
    }
}

static inline __attribute__((noreturn)) void panic_at(
    const char *message, const char *file, uint32_t line,
    const struct kernel_panic_registers *saved_registers) {
    struct kernel_panic_registers captured;
    const struct kernel_panic_registers *registers = saved_registers;

    if (registers == 0) {
        __asm__ volatile (
                        "movl %%edi, %0\n\t"
                        "movl %%esi, %1\n\t"
                        "movl %%ebp, %2\n\t"
                        "movl %%esp, %3\n\t"
                        "movl %%ebx, %4\n\t"
                        "movl %%edx, %5\n\t"
                        "movl %%ecx, %6\n\t"
                        "movl %%eax, %7\n\t"
                        : "=m" (captured.edi), "=m" (captured.esi),
                            "=m" (captured.ebp), "=m" (captured.esp),
                            "=m" (captured.ebx), "=m" (captured.edx),
                            "=m" (captured.ecx), "=m" (captured.eax)
            : : "memory");
        __asm__ volatile ("pushfl; popl %0" : "=m" (captured.eflags)
                          : : "memory");
        captured.eip = (uint32_t)(uintptr_t)__builtin_return_address(0);
        captured.cs = 0;
        captured.gs = 0;
        captured.fs = 0;
        captured.es = 0;
        captured.ds = 0;
        captured.vector = 0xFFFFFFFFu;
        captured.error_code = 0;
        registers = &captured;
    }

    k_set_quiet(0);
    beep(990, 150); // Why not beep when the kernel panics?
    fb_panic_screen(message);
    k_print("KERNEL PANIC!!\n");
    k_print(message);
    k_print("\nFile: ");
    k_print(file);
    k_print("\nLine: ");
    panic_print_decimal(line);
    k_print("\nRegisters: ");
    panic_print_hex("EAX", registers->eax);
    panic_print_hex("EBX", registers->ebx);
    panic_print_hex("ECX", registers->ecx);
    panic_print_hex("EDX", registers->edx);
    k_print("\n           ");
    panic_print_hex("ESI", registers->esi);
    panic_print_hex("EDI", registers->edi);
    panic_print_hex("EBP", registers->ebp);
    panic_print_hex("ESP", registers->esp);
    k_print("\n           ");
    panic_print_hex("EIP", registers->eip);
    panic_print_hex("EFLAGS", registers->eflags);
    k_print("\n           ");
    panic_print_hex("CS", registers->cs);
    panic_print_hex("DS", registers->ds);
    panic_print_hex("ES", registers->es);
    panic_print_hex("FS", registers->fs);
    panic_print_hex("GS", registers->gs);
    if (registers->vector != 0xFFFFFFFFu) {
        k_print("\n           ");
        panic_print_hex("VECTOR", registers->vector);
        panic_print_hex("ERROR", registers->error_code);
    }
    k_print("\n");
    k_print("END KERNEL PANIC -- ");
    k_print(message);

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}

#define KERNEL_PANIC_SELECT(_0, _1, NAME, ...) NAME
#define KERNEL_PANIC_DEFAULT() \
    panic_at("Kernel panic", __FILE__, __LINE__, 0)
#define KERNEL_PANIC_MESSAGE(message) \
    panic_at((message), __FILE__, __LINE__, 0)
#define panic(...) \
    KERNEL_PANIC_SELECT(0, ##__VA_ARGS__, KERNEL_PANIC_MESSAGE, \
                        KERNEL_PANIC_DEFAULT)(__VA_ARGS__)

#endif