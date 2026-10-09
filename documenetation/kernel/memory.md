# Memory checks and allocation status

The kernel has a simple loader address check in `check_memory_quota(addr,
size)`. It rejects unsigned range wraparound and ranges that end above 16 MiB.
This is a guard on selected binary-loader paths, not a physical-memory map,
allocator, paging system, or per-process memory quota.

There are no per-process address spaces or memory protection boundaries.
Userspace executables run at kernel privilege in the shared address space, and
syscall pointers are generally trusted. A malformed or hostile program can
corrupt the kernel even if its load range passed the quota check.

`oom_kill()` currently logs the requested range and panics; it does not
reclaim memory or kill an isolated process. See [Programs and processes](processes.md)
for loader constraints and [Kernel panic](panic.md) for failure behavior.
