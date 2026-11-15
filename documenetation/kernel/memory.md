# Paging, memory allocation, and checks

The x86 boot path now enables 4 MiB identity-mapped paging and initializes a
physical page-frame bitmap from the Multiboot memory map. `memory_alloc_page()`
and `memory_alloc_pages()` allocate physical pages; matching free calls return
them to the bitmap. `/proc/meminfo` and `/sys/memory/` expose the usable and
free page counts.

The current map is a supervisor-only identity map of the 32-bit physical
address space. This is kernel paging, not process isolation: all programs
continue to execute at kernel privilege in a shared address space. The
allocator reserves the kernel image, Multiboot metadata, and initramfs module.

The older loader check `check_memory_quota(addr, size)` still rejects
wraparound and ranges ending above 16 MiB. It is not a general allocation
policy or per-process memory quota.

There are no per-process address spaces or memory protection boundaries.
Userspace executables run at kernel privilege in the shared address space, and
syscall pointers are generally trusted. A malformed or hostile program can
corrupt the kernel even if its load range passed the quota check.

The kernel publishes a page-aligned, versioned vDSO-like ABI page for a shared
syscall trampoline and a direct `getpid()` fast path. It is not independently
mapped or protected per process; it shares the current identity-mapped kernel
address space and must not be treated as a security boundary.

`oom_kill()` still logs the requested range and panics; it does not reclaim
memory or kill an isolated process. See [Programs and processes](processes.md)
for loader constraints and [Kernel panic](panic.md) for failure behavior.


## Aligned page allocation

The physical page allocator also provides `memory_alloc_pages_aligned(count, alignment_pages)`.
The alignment must be a nonzero power of two, expressed in 4 KiB pages. The
existing `memory_alloc_pages(count)` API remains available and uses one-page
alignment. `memory_used_pages()` reports usable pages currently marked allocated.

## Privilege transition prerequisites

OpenKernel's GDT/TSS setup defines ring 0 and ring 3 code/data selectors and
loads a TSS with a kernel-entry stack. The ring-3 entry helper requires its
entry point and stack to be mapped with user permissions. The current identity
mapping is supervisor-only, so this change establishes segmentation and the
privilege-transition interface; it does not yet make existing executable
loading or processes isolated ring-3 tasks.

## Per-process address spaces

The virtual-memory layer now provides independent page directories cloned from
the kernel's supervisor-only 4 MiB identity map. Each address space can map,
unmap, and change permissions on individual 4 KiB pages in the user virtual
window `0xC0000000-0xFFFFFFFF`. Page-table entries, rather than only directory
entries, carry the user-accessible bit, so a user mapping does not make the
other identity-mapped pages in that 4 MiB region accessible at CPL3.

The top 1 GiB of physical memory is reserved by the physical allocator because
the current kernel uses an identity map. This keeps physical kernel allocations
from colliding with the user virtual window. The remaining physical memory is
identity mapped for kernel use.

Processes created through `process_enter_program()` receive a distinct address
space. Returning from the program restores the parent's address space, and the
interrupt scheduler switches address spaces when selecting another process.
Passing a null address-space pointer selects the kernel page directory.

This is the virtual-memory foundation, not a complete userspace loader:
executable segments and user stacks are not mapped automatically, fork does
not yet duplicate user mappings, and page faults are reported as kernel panics
with the faulting CR2 address and x86 error code. Do not enter ring 3 until the
program image and stack have been mapped with `VMM_PAGE_USER` permissions.
