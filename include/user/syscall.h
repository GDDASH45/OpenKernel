#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>
#include <kernel/syscall.h>

/*
 * OpenKernel's current 32-bit software-interrupt ABI:
 * EAX = syscall number; EBX, ECX, EDX = arguments; EAX = result.
 * Negative results are -errno values. These helpers require i386 code.
 */
static inline int32_t ok_syscall3(uint32_t number, uint32_t arg1,
                                  uint32_t arg2, uint32_t arg3) {
    int32_t result;
    __asm__ volatile ("int $0x80"
                      : "=a" (result)
                      : "0" (number), "b" (arg1), "c" (arg2), "d" (arg3)
                      : "memory", "cc");
    return result;
}

static inline int32_t ok_write(int fd, const void *buffer, uint32_t size) {
    const uint8_t *bytes = (const uint8_t *)buffer;
    uint32_t written = 0;

    while (written < size) {
        int32_t result = ok_syscall3(OK_SYS_WRITE, (uint32_t)fd,
                                     (uint32_t)(uintptr_t)(bytes + written),
                                     size - written);
        if (result < 0) {
            return written != 0 ? (int32_t)written : result;
        }
        if (result == 0) {
            break;
        }
        written += (uint32_t)result;
    }
    return (int32_t)written;
}

static inline int32_t ok_console_write(const void *buffer, uint32_t size) {
    const uint8_t *bytes = (const uint8_t *)buffer;
    uint32_t written = 0;

    if (buffer == 0 && size != 0) {
        return -14;
    }
    while (written < size) {
        int32_t result = ok_syscall3(
            OK_SYS_CONSOLE_WRITE,
            (uint32_t)(uintptr_t)(bytes + written), size - written, 0);
        if (result < 0) {
            return written != 0 ? (int32_t)written : result;
        }
        if (result == 0) {
            break;
        }
        written += (uint32_t)result;
    }
    return (int32_t)written;
}

static inline int32_t ok_putchar(char character) {
    return ok_console_write(&character, 1);
}

static inline int32_t ok_puts(const char *text) {
    uint32_t length = 0;
    int32_t result;

    if (text == 0) {
        return -14;
    }
    while (text[length] != '\0') {
        length++;
    }
    result = ok_console_write(text, length);
    if (result < 0) {
        return result;
    }
    result = ok_putchar('\n');
    return result < 0 ? result : (int32_t)(length + 1);
}

static inline int32_t ok_getpid(void) {
    return ok_syscall3(OK_SYS_GETPID, 0, 0, 0);
}

static inline int32_t ok_yield(void) {
    return ok_syscall3(OK_SYS_YIELD, 0, 0, 0);
}

#endif
