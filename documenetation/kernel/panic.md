# Kernel panic API

Kernel code can include `<kernel.h>` and stop execution with:

```c
panic("Reason for the unrecoverable failure");
```

The macro records the source file and line where it was called. The panic
handler enables normal logging, requests a short PC-speaker beep, paints a
minimal black framebuffer screen with a white accent when the framebuffer is
available, prints the message and diagnostic register values through the
kernel console, then halts the CPU in a loop. The framebuffer backdrop itself
does not draw panic text; text output is a separate console operation.

CPU exceptions enter the panic path with the saved interrupt frame, including
the exception vector and error code. For an ordinary `panic()` call, the
reported instruction address is a return address and not a decoded stack
trace. See [Interrupts and exceptions](interrupts.md) for diagnostic details.

An assertion failure has its own handler in `kernel/assert.c`; it prints the
assertion expression, source file, and line and halts. It does not currently
use the general panic register dump.