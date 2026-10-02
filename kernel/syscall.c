#include <stdint.h>
#include <kernel/syscall.h>
#include <kernel/process.h>
#include "../fs/vfs.h"
#include <write/write.h>

#define OK_EBADF 9
#define OK_EFAULT 14
#define OK_EIO 5
#define OK_ENOSYS 38
#define OK_SYSCALL_WRITE_MAX 4096u

extern int execve(const char *filename, char *const argv[],
                  char *const envp[]);

static int32_t console_write(const char *buffer, uint32_t size) {
    int result;

    if (buffer == 0 && size != 0) {
        return -OK_EFAULT;
    }
    if (size > OK_SYSCALL_WRITE_MAX) {
        size = OK_SYSCALL_WRITE_MAX;
    }
    if (size == 0) {
        return 0;
    }

    result = vfs_write_file(vfs_get_root(), "/device/console", buffer, size);
    return result < 0 ? -OK_EIO : result;
}

int32_t syscall_dispatch(uint32_t number, uint32_t arg1, uint32_t arg2,
                         uint32_t arg3) {
    switch (number) {
    case OK_SYS_WRITE: {
        const char *buffer = (const char *)(uintptr_t)arg2;
        if (arg1 != OK_STDOUT_FILENO && arg1 != OK_STDERR_FILENO) {
            return -OK_EBADF;
        }
        return console_write(buffer, arg3);
    }
    case OK_SYS_CONSOLE_WRITE:
        return console_write((const char *)(uintptr_t)arg1, arg2);
    case OK_SYS_GETPID:
        return (int32_t)process_current_pid();
    case OK_SYS_YIELD:
        __asm__ volatile ("pause" ::: "memory");
        return 0;
    case OK_SYS_FORK:
        /* Do not fake fork semantics before processes have separate state. */
        return -OK_ENOSYS;
    case OK_SYS_EXECVE:
        return execve((const char *)(uintptr_t)arg1,
                      (char *const *)(uintptr_t)arg2,
                      (char *const *)(uintptr_t)arg3);
    default:
        (void)arg1;
        (void)arg2;
        (void)arg3;
        return -OK_ENOSYS;
    }
}
