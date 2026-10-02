#include <stdint.h>
#include <kernel/syscall.h>
#include <write/write.h>

#define OK_EBADF 9
#define OK_EFAULT 14
#define OK_ENOSYS 38
#define OK_SYSCALL_WRITE_MAX 4096u

int32_t syscall_dispatch(uint32_t number, uint32_t arg1, uint32_t arg2,
                         uint32_t arg3) {
    switch (number) {
    case OK_SYS_WRITE: {
        const char *buffer = (const char *)(uintptr_t)arg2;
        uint32_t size = arg3;

        if (arg1 != OK_STDOUT_FILENO && arg1 != OK_STDERR_FILENO) {
            return -OK_EBADF;
        }
        if (buffer == 0 && size != 0) {
            return -OK_EFAULT;
        }
        if (size > OK_SYSCALL_WRITE_MAX) {
            size = OK_SYSCALL_WRITE_MAX;
        }
        for (uint32_t index = 0; index < size; index++) {
            k_print_char(buffer[index]);
        }
        return (int32_t)size;
    }
    case OK_SYS_GETPID:
        /* Process management is not implemented yet; 1 identifies init. */
        return 1;
    case OK_SYS_YIELD:
        __asm__ volatile ("pause" ::: "memory");
        return 0;
    default:
        (void)arg1;
        (void)arg2;
        (void)arg3;
        return -OK_ENOSYS;
    }
}
