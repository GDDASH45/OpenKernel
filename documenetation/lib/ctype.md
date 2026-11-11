# Character Classification Library

The OpenKernel Character Classification Library provides ASCII character testing and conversion functions.

**Source:** `lib/ctype.c`  
**Header:** `include/kernel/ctype.h`

## Character Classification

| Function | Description |
|---|---|
| `isalnum()` | Checks for letters or digits. |
| `isalpha()` | Checks for alphabetic characters. |
| `isascii()` | Checks whether a character is ASCII. |
| `isblank()` | Checks for spaces and horizontal tabs. |
| `iscntrl()` | Checks for control characters. |
| `isdigit()` | Checks for decimal digits. |
| `isgraph()` | Checks for printable non-space characters. |
| `islower()` | Checks for lowercase letters. |
| `isprint()` | Checks for printable characters. |
| `ispunct()` | Checks for punctuation characters. |
| `isspace()` | Checks for whitespace. |
| `isupper()` | Checks for uppercase letters. |
| `isxdigit()` | Checks for hexadecimal digits. |

## Character Conversion

| Function | Description |
|---|---|
| `tolower()` | Converts uppercase letters to lowercase. |
| `toupper()` | Converts lowercase letters to uppercase. |

## Usage

Include the header:

```c
#include <kernel/ctype.h>
```

### Classification example

```c
#include <kernel/ctype.h>

void check_character(char c)
{
    if (isalpha(c)) {
        /* Character is a letter. */
    }

    if (isdigit(c)) {
        /* Character is a decimal digit. */
    }

    if (isxdigit(c)) {
        /* Character is hexadecimal. */
    }
}
```

### Conversion example

```c
#include <kernel/ctype.h>

void convert_character(void)
{
    int upper = toupper('a');
    int lower = tolower('Z');

    /* upper = 'A' */
    /* lower = 'z' */
}
```

## Implementation Details

- Uses ASCII character ranges.
- Supports characters from `0x00` through `0x7F`.
- Does not require locale support.
- Classification functions return nonzero when a condition matches.
- Unsupported character values are not classified.
- Conversion functions leave unsupported values unchanged.
- This implementation does not provide Unicode classification.

