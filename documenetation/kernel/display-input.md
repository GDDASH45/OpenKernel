# Display and input

## Linear framebuffer

At boot the kernel accepts a Multiboot RGB framebuffer with valid dimensions
and 15–32 bits per pixel. It converts RGB values to the reported channel
layout and provides pixel, rectangle, clear, and text rendering for kernel
components. A framebuffer console (`fbcon`) uses a fixed 8×10 font and
character-cell backing buffer. Userspace has separate drawing syscalls; it
cannot map the framebuffer directly or change the video mode.

See [Framebuffer API](framebuffer.md) for userspace wrappers, parameters,
examples, and limits. The panic display is a minimal black background with a
red accent; the panic message is printed through the normal kernel console
path, not painted by `fb_panic_screen()`.

Boot also attempts to draw and fade an embedded BMP logo when framebuffer
initialization succeeds. BMP handling is a simple kernel display feature, not
a userspace image API.

## Keyboard

The PS/2 keyboard driver handles IRQ1, translates a limited scan-code set to
ASCII-like characters, and buffers input in a fixed ring. Shift and caps-lock
are supported. `keyboard_try_get_char()` is nonblocking; the syscall
`ok_getchar()` loops and yields while no character is queued. The userspace
`ok_input_dispatch(handler, context)` helper repeatedly delivers characters to
a callback. Extended navigation keys and full keyboard layouts are not
provided.

## Mouse

The PS/2 mouse driver handles IRQ12 and three-byte packets. It clamps its
coordinates to the supplied screen dimensions and exposes polling through
`mouse_poll()`. There is no userspace mouse syscall. The desktop helper uses
mouse state when run, but normal startup does not enter the desktop loop.

## Desktop helpers

`desktop_show()` draws a static desktop and `desktop_run()` polls and redraws
mouse interaction. They are kernel-side routines in `kernel/desktop.c` and
are not invoked by the normal boot path. They should be treated as experimental
rendering code, not a complete window manager or userspace desktop service.
