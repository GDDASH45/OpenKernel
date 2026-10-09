# Userspace framebuffer API

Userspace programs can draw through the framebuffer syscalls declared in
`<user/syscall.h>`. Include that header (or a userspace header that includes
it) and use the `ok_fb_*` wrappers; they invoke the kernel through `int $0x80`.
These calls operate on the linear framebuffer selected during boot. They do
not expose its physical address to userspace.

## Query display information

Call `ok_fb_get_info()` to obtain the display dimensions and pixel layout:

```c
#include <user/syscall.h>

struct ok_fb_info display;
if (ok_fb_get_info(&display) < 0) {
    /* No usable linear framebuffer is available. */
}
```

`struct ok_fb_info` contains `width`, `height`, `pitch` (bytes per scanline),
and `bits_per_pixel`. A framebuffer may include padding at the end of each
scanline, so use `pitch` rather than assuming `width * bytes_per_pixel` when
writing raw data.

## Clear and draw pixels

`ok_fb_clear(rgb)` fills the display with one RGB color. `ok_fb_put_pixel(x, y,
rgb)` writes one pixel. Colors are supplied as `0xRRGGBB`; coordinates begin at
`(0, 0)` in the upper-left corner.

```c
if (ok_fb_clear(0x101820) < 0 ||
    ok_fb_put_pixel(20, 30, 0xFF8040) < 0) {
    /* The framebuffer is unavailable or the pixel coordinate is invalid. */
}
```

The pixel syscall converts RGB into the framebuffer's configured channel
layout. Pixel coordinates outside the reported width or height fail.

## Write raw framebuffer bytes

`ok_fb_write(offset, buffer, size)` copies bytes to the framebuffer, where
`offset` is a byte offset from its beginning. Raw writes use the display's
native pixel format and scanline pitch. Each call is limited to 1 MiB; split
larger updates into chunks and ensure every range fits within
`pitch * height` bytes.

```c
/* For example, copy a prepared pixel row at the start of scanline 40. */
uint32_t bytes_per_pixel = (display.bits_per_pixel + 7) / 8;
uint32_t row_bytes = display.width * bytes_per_pixel;
uint32_t offset = 40 * display.pitch;
int32_t result = ok_fb_write(offset, row, row_bytes);
```

The byte row must already be encoded in the framebuffer's native channel
layout; use `ok_fb_put_pixel()` when a portable RGB operation is more
convenient. The sample row must fit the display's pitch.

## Return values and limits

The wrappers return `0` on success for clear, pixel, and info operations. Raw
writes return the number of bytes copied. Errors are negative errno-style
values; `-OK_EIO` indicates an unavailable framebuffer or an invalid raw byte
range, `-OK_EFAULT` indicates a null required pointer, and `-22` indicates a
write larger than the syscall limit. The framebuffer syscalls currently
provide drawing only—there is no syscall for changing video mode or mapping
the framebuffer directly into a userspace address space.
