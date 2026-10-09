# Interrupts, exceptions, and panic handling

## IDT and PIC

The kernel installs 32 CPU exception gates, remaps the dual 8259 PIC to IRQ
vectors 32–47, and installs the userspace-callable syscall gate at vector
`0x80`. The assembly stubs save general registers and segment selectors before
calling the C dispatcher. The keyboard driver handles IRQ1 and the mouse driver
handles IRQ12; other IRQs are acknowledged but have no device service. The PIC
starts with IRQs masked and drivers enable the interrupts they need.

## CPU exceptions

A CPU exception is reported as a kernel panic. The saved exception frame
includes general registers, instruction pointer, flags, vector, and error code
where applicable. Ordinary `panic()` calls include their macro call-site file
and line and capture a useful register snapshot, but their EIP is a return
address and their selector/vector fields are unavailable. Register output is
for diagnosis, not a symbolized stack trace.

See [Kernel panic](panic.md) for the panic API and display behavior. The IDT,
PIC, stubs, and dispatch are implemented in `kernel/interrupts.c` and
`kernel/interrupts.asm`.

## Assertions

The project assertion handler prints the assertion expression, source file,
and line, then halts. Warning reports include file and line but return to the
caller. Assertions do not currently route through the general register-dump
panic path.
