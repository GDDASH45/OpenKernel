# Contributing to OpenKernel

Thank you for your interest in contributing! OpenKernel implementation and
documentation live in the repository; start with the
[feature documentation index](documenetation/index.md) and check the relevant
subsystem page before changing interfaces.

- **Kernel and userspace code:** Use freestanding C for general implementation.
- **Assembly:** Keep architecture-specific assembly limited to code that needs
	it, such as boot entry, interrupt stubs, or context switching. The project
	already contains x86 assembly for those purposes.
- **Documentation:** Update the relevant page under `documenetation/` when
	changing or adding a user-visible feature. Describe current limitations and
	avoid documenting planned behavior as implemented.
- **Validation:** Build the kernel and run focused checks appropriate to the
	change. Do not imply that a successful compile validates runtime behavior.