# String Library

The OpenKernel String Library provides basic string manipulation and memory operations for kernel components.

**Source:** `lib/string.c`  
**Header:** `include/kernel/string.h`

## String Operations

| Function | Description |
|---|---|
| `strlen()` | Returns the length of a null-terminated string. |
| `strcmp()` | Compares two strings. |
| `strncmp()` | Compares two strings up to a specified length. |
| `strcpy()` | Copies a string into another buffer. |
| `strncpy()` | Copies a specified number of characters. |
| `strcat()` | Appends one string to another. |
| `strchr()` | Finds the first occurrence of a character. |
| `strrchr()` | Finds the last occurrence of a character. |
| `strstr()` | Finds the first occurrence of a substring. |

## Memory Operations

| Function | Description |
|---|---|
| `memset()` | Fills a memory region with a specified byte value. |
| `memcpy()` | Copies a memory region. |
| `memcmp()` | Compares two memory regions. |
| `memmove()` | Copies memory regions while handling overlapping buffers. |

## Header Usage

Include the library header in kernel source files:

```c
#include <kernel/string.h>
```

## Example

```c
#include <kernel/string.h>

void example(void)
{
    char buffer[32];

    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "OpenKernel");

    if (strcmp(buffer, "OpenKernel") == 0) {
        /* Strings match. */
    }
}
```

## Notes

- These functions are implemented by OpenKernel.
- The library is intended for kernel-space use.
- Functions operate on caller-provided memory; callers must ensure buffers are large enough.
- `memcpy()` requires non-overlapping source and destination regions. Use `memmove()` when regions may overlap.
- Compile the implementation with appropriate freestanding settings and `-fno-builtin` to prevent compiler built-in substitutions from causing recursive calls.

