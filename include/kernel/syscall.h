#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#define OK_SYS_WRITE  1u
#define OK_SYS_GETPID  2u
#define OK_SYS_YIELD  3u

#define OK_STDIN_FILENO  0
#define OK_STDOUT_FILENO 1
#define OK_STDERR_FILENO 2

int32_t syscall_dispatch(uint32_t number, uint32_t arg1, uint32_t arg2,
						 uint32_t arg3);

#endif
