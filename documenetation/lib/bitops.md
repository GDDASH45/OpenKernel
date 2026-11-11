# Bitmap Operations

OpenKernel provides bit manipulation functions for working with individual bits in 32-bit bitmap words. These operations are useful for managing flags, allocation maps, and other kernel data structures.

**Source:** `lib/bitopts.c`  
**Header:** `include/kernel/bitops.h`

## Functions

| Function | Description |
|---|---|
| `set_bit()` | Sets a bit to `1`. |
| `clear_bit()` | Clears a bit to `0`. |
| `test_bit()` | Tests whether a bit is set. |
| `test_and_set_bit()` | Tests a bit and then sets it. |
| `test_and_clear_bit()` | Tests a bit and then clears it. |

## Bit Numbering

Bits are numbered starting at zero.

For a 32-bit bitmap word:

| Bit range | Word |
|---|---|
| `0–31` | Word 0 |
| `32–63` | Word 1 |
| `64–95` | Word 2 |
| `96–127` | Word 3 |

The word index and bit offset are calculated using:

```c
word_index = bit / 32;
bit_offset = bit % 32;
```

## Usage

Include the header:

```c
#include <kernel/bitops.h>
```

### Setting a bit

```c
set_bit(5, bitmap);
```

### Clearing a bit

```c
clear_bit(5, bitmap);
```

### Testing a bit

```c
if (test_bit(5, bitmap)) {
    /* Bit 5 is set. */
}
```

### Test and set

```c
if (!test_and_set_bit(5, bitmap)) {
    /* Bit 5 was previously clear. */
}
```

### Test and clear

```c
if (test_and_clear_bit(5, bitmap)) {
    /* Bit 5 was previously set. */
}
```

## Implementation Notes

- Bitmap words use `uint32_t`.
- Bit numbering starts at zero.
- Operations are non-atomic.
- Callers must ensure the bitmap has enough storage for the requested bit index.
- Concurrent access requires external synchronization.

