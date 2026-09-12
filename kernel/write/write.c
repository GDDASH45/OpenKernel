#include <write/write.h>

static int cursor_row = 0;
static int cursor_col = 0;

void k_clear_screen(void) {
    volatile unsigned char *vga = (volatile unsigned char *)VGA_ADDRESS;
    for (int i = 0; i < MAX_ROWS * MAX_COLS; i++) {
        vga[i * 2] = ' ';
        vga[i * 2 + 1] = WHITE_ON_BLACK;
    }
    cursor_row = 0;
    cursor_col = 0;
}

void k_print_char(char c) {
    volatile unsigned char *vga = (volatile unsigned char *)VGA_ADDRESS;

    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else {
        int index = (cursor_row * MAX_COLS + cursor_col) * 2;
        vga[index] = c;
        vga[index + 1] = WHITE_ON_BLACK;
        cursor_col++;

        if (cursor_col >= MAX_COLS) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= MAX_ROWS) {
        cursor_row = 0; // Wrap back to top (or implement scrolling logic)
    }
}

void k_print(const char *str) {
    int i = 0;
    while (str[i] != '\0') {
        k_print_char(str[i]);
        i++;
    }
}