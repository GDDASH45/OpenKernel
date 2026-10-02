#ifndef USER_PROCESS_H
#define USER_PROCESS_H

#include <stdint.h>
#include <kernel/syscall.h>
#include <user/syscall.h>

/*
 * Process ABI wrappers for OpenKernel's current 32-bit int 0x80 interface.
 * fork() currently returns -ENOSYS until the kernel has isolated address
 * spaces and a scheduler. execve() and launch() load a named initramfs image.
 */
static inline int32_t fork(void) {
    return ok_syscall3(OK_SYS_FORK, 0, 0, 0);
}

static inline int32_t getpid(void) {
    return ok_getpid();
}

static inline int32_t execve(const char *path, char *const argv[],
                             char *const envp[]) {
    return ok_syscall3(OK_SYS_EXECVE,
                       (uint32_t)(uintptr_t)path,
                       (uint32_t)(uintptr_t)argv,
                       (uint32_t)(uintptr_t)envp);
}

static inline int32_t launch(const char *path, char *const argv[],
                             char *const envp[]) {
    return execve(path, argv, envp);
}

#endif
