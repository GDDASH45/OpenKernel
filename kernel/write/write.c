#include <write/write.h>

static int cursor_row = 0;
static int cursor_col = 0;

#define VGA_ADDRESS 0xB8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0F

void scroll_screen(void) {
    volatile unsigned char *vga = (volatile unsigned char *)VGA_ADDRESS;

    // Shift every row up by one (each cell is 2 bytes: char + attribute)
    for (int r = 1; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            int dest = ((r - 1) * MAX_COLS + c) * 2;
            int src = (r * MAX_COLS + c) * 2;
            vga[dest] = vga[src];
            vga[dest + 1] = vga[src + 1];
        }
    }

    // Clear the bottom row
    for (int c = 0; c < MAX_COLS; c++) {
        int index = ((MAX_ROWS - 1) * MAX_COLS + c) * 2;
        vga[index] = ' ';
        vga[index + 1] = WHITE_ON_BLACK;
    }

    cursor_row = MAX_ROWS - 1;
}

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
        scroll_screen();
    }
}

void k_print(const char *str) {
    int i = 0;
    while (str[i] != '\0') {
        k_print_char(str[i]);
        i++;
    }
}