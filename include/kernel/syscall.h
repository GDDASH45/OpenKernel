#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <stdint.h>


#define OK_SYS_EXIT 0u
#define OK_SYS_WRITE  1u
#define OK_SYS_GETPID  2u
#define OK_SYS_YIELD  3u
#define OK_SYS_CONSOLE_WRITE 4u
#define OK_SYS_FORK   5u
#define OK_SYS_EXECVE 6u
#define OK_SYS_GETCHAR 7u
#define OK_SYS_CLEAR  8u
#define OK_SYS_MKDEV  9u
#define OK_SYS_READDIR 10u
#define OK_SYS_LOAD_OSO 11u

#define OK_DEVICE_CONSOLE 1u
#define OK_DEVICE_NULL    2u
#define OK_DEVICE_ZERO    3u
#define OK_DEVICE_RANDOM  4u
#define OK_DEVICE_KEYBOARD 5u

#define OK_STDIN_FILENO  0
#define OK_STDOUT_FILENO 1
#define OK_STDERR_FILENO 2

#define OK_EAGAIN 11
#define OK_EBADF  9
#define OK_EFAULT 14
#define OK_EIO    5
#define OK_ENOSYS 38

int32_t syscall_dispatch(uint32_t number, uint32_t arg1, uint32_t arg2,
						 uint32_t arg3);

#endif
