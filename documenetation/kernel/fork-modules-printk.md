# Fork, module syscalls, and kernel logging

## Process fork

`fork()` clones the active process's 16 KiB kernel stack and each present
user-marked page in its address space. The parent receives the child PID and
the child receives zero. The child becomes runnable at the next cooperative
scheduler yield. User pages are copied eagerly; this is not copy-on-write.
Kernel mappings remain shared. The process model still has no timer
preemption or `wait()` syscall.

## Kernel module insertion

Syscall IDs 21 and 22 are named `OK_SYS_MODULE_INSERT` and
`OK_SYS_MODULE_REMOVE`. The previous `OK_SYS_MODULE_LOAD` and
`OK_SYS_MODULE_UNLOAD` names remain aliases. Userspace wrappers are
`ok_module_insert(path)` and `ok_module_remove(name)`, with the previous
wrapper names retained for compatibility.

Insertion loads an i386 ELF32 relocatable `.ko` from the initramfs. Both
operations are restricted to UID 0 and UID_SROOT. The syscall layer copies
the supplied string from a mapped user page into a bounded kernel buffer
before using it. Module code still runs at kernel privilege; only trusted
modules should be loaded.

## Timestamped printk

`printk(format, ...)` writes a timestamp followed by the formatted message.
The timestamp is read from the CMOS RTC and has the form
`[YYYY-MM-DD HH:MM:SS]`. If the RTC cannot be read, the prefix is
`[time unavailable]`. Existing `k_print()` behavior is unchanged.
