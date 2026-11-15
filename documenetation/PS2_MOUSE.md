# PS/2 Mouse Driver

OpenKernel's PS/2 auxiliary-device driver is implemented in
`sdrivers/mouse/mouse.c`. Its public interface is
`include/driver/mouse.h`.

## Supported devices

- Standard PS/2 mice using three-byte packets.
- IntelliMouse-compatible wheel devices using four-byte packets (device ID 3).
- IntelliMouse Explorer-compatible devices using four-byte packets and the
  fourth/fifth button bits (device ID 4).
- Left, right, middle, fourth, and fifth button state.
- Relative X/Y movement, wheel movement, and button transition events.

The driver starts with standard three-byte packet parsing. It negotiates wheel
mode with the sample-rate sequence 200, 100, 80 and reads the device ID. If
negotiation is unsupported or times out, it keeps standard packet parsing.

## Initialization

Call `init_mouse(width, height)` after the IDT and PIC have been initialized.
The function enables the auxiliary port, configures IRQ12, resets the device,
attempts wheel-mode negotiation, enables reporting, initializes the cursor to
the center of the supplied bounds, and unmasks IRQ12.

The IRQ dispatcher must call `mouse_irq_handler()` for IRQ12. The handler
consumes auxiliary bytes only, assembles complete packets, and performs bounded
work per interrupt.

## Polling

`mouse_poll(&state)` returns 1 when one or more complete packets have arrived
since the previous poll, and 0 when no new event is available. It copies a
consistent snapshot while interrupts are disabled briefly.

- `x`, `y`: clamped cursor position.
- `buttons`: currently held button bitmask.
- `pressed`, `released`: button transitions accumulated since the previous
  poll.
- `delta_x`, `delta_y`: accumulated relative movement since the previous
  poll. Positive Y is downward on the screen.
- `wheel`: accumulated signed wheel steps since the previous poll.
- `packets`: number of complete packets accumulated since the previous poll.

The event fields are cleared after a successful poll; cursor position and
current button state remain persistent.

Button masks are exposed as `MOUSE_BUTTON_LEFT`, `MOUSE_BUTTON_RIGHT`,
`MOUSE_BUTTON_MIDDLE`, `MOUSE_BUTTON_4`, and `MOUSE_BUTTON_5`.

## Runtime controls

- `mouse_set_bounds(width, height)` updates the coordinate limits and clamps
  the current position.
- `mouse_set_position(x, y)` moves the logical cursor and publishes a new
  state generation.

The driver does not draw a cursor. Framebuffer or desktop code owns rendering
and should use the state returned by `mouse_poll()`.

## Limitations

This is a legacy i8042/PS/2 driver. It does not support USB HID mice, absolute
touch devices, hot-plug discovery, or a general input-event queue. Controller
initialization assumes the platform exposes a working PS/2 auxiliary port.
