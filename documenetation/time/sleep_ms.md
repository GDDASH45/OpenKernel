# sleep_ms

Kernel code can include `<kernel/time.h>` and call:

```c
sleep_ms(100); /* Approximate busy-loop delay; not wall-clock accurate. */
```

The function runs nested loops containing `nop` instructions. It does not
program a timer, block a scheduled task, or guarantee a particular elapsed
time across CPU types and emulator speeds. There is no userspace sleep syscall.
See [Sound and timing](../kernel/sound-time.md) for current timing limitations.