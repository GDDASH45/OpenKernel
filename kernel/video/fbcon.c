#include <kvideo/fb.h>
#include <kvideo/fbcon.h>
#include <kvideo/font8x10.h>

#define FBCON_MAX_COLUMNS 256
#define FBCON_MAX_ROWS 128
#define FBCON_TAB_WIDTH 4
#define FBCON_FOREGROUND 0xF5F7FA
#define FBCON_BACKGROUND 0x101820

static char cells[FBCON_MAX_ROWS][FBCON_MAX_COLUMNS];
static uint32_t columns;
static uint32_t rows;
static uint32_t cursor_column;
static uint32_t cursor_row;
static int active;

static void draw_cell(uint32_t column, uint32_t row) {
    int x = (int)(column * FONT8X10_WIDTH);
    int y = (int)(row * FONT8X10_HEIGHT);
    char character = cells[row][column];

    fb_fill_rect(x, y, FONT8X10_WIDTH, FONT8X10_HEIGHT,
                 FBCON_BACKGROUND);
    if (character != ' ') {
        fb_draw_char(x, y, character, FBCON_FOREGROUND);
    }
}

static void redraw(void) {
    for (uint32_t row = 0; row < rows; row++) {
        for (uint32_t column = 0; column < columns; column++) {
            draw_cell(column, row);
        }
    }
}

static void scroll(void) {
    for (uint32_t row = 1; row < rows; row++) {
        for (uint32_t column = 0; column < columns; column++) {
            cells[row - 1][column] = cells[row][column];
        }
    }
    for (uint32_t column = 0; column < columns; column++) {
        cells[rows - 1][column] = ' ';
    }
    cursor_row = rows - 1;
    redraw();
}

static void advance_line(void) {
    cursor_column = 0;
    cursor_row++;
    if (cursor_row >= rows) {
        scroll();
    }
}

int fbcon_init(void) {
    uint32_t width = fb_width();
    uint32_t height = fb_height();

    active = 0;
    if (width < FONT8X10_WIDTH || height < FONT8X10_HEIGHT) {
        return -1;
    }

    columns = width / FONT8X10_WIDTH;
    rows = height / FONT8X10_HEIGHT;
    if (columns > FBCON_MAX_COLUMNS) {
        columns = FBCON_MAX_COLUMNS;
    }
    if (rows > FBCON_MAX_ROWS) {
        rows = FBCON_MAX_ROWS;
    }

    cursor_column = 0;
    cursor_row = 0;
    for (uint32_t row = 0; row < rows; row++) {
        for (uint32_t column = 0; column < columns; column++) {
            cells[row][column] = ' ';
        }
    }
    active = 1;
    fb_clear(FBCON_BACKGROUND);
    return 0;
}

int fbcon_is_active(void) {
    return active;
}

void fbcon_clear(void) {
    if (!active) {
        return;
    }

    for (uint32_t row = 0; row < rows; row++) {
        for (uint32_t column = 0; column < columns; column++) {
            cells[row][column] = ' ';
        }
    }
    cursor_column = 0;
    cursor_row = 0;
    fb_clear(FBCON_BACKGROUND);
}

void fbcon_put_char(char character) {
    if (!active) {
        return;
    }

    if (character == '\n') {
        advance_line();
        return;
    }
    if (character == '\r') {
        cursor_column = 0;
        return;
    }
    if (character == '\b') {
        if (cursor_column > 0) {
            cursor_column--;
            cells[cursor_row][cursor_column] = ' ';
            draw_cell(cursor_column, cursor_row);
        }
        return;
    }
    if (character == '\t') {
        uint32_t spaces = FBCON_TAB_WIDTH -
                          (cursor_column % FBCON_TAB_WIDTH);
        while (spaces-- > 0) {
            fbcon_put_char(' ');
        }
        return;
    }

    cells[cursor_row][cursor_column] = character;
    draw_cell(cursor_column, cursor_row);
    cursor_column++;
    if (cursor_column >= columns) {
        advance_line();
    }
}

int fbcon_write(const char *buffer, uint32_t size) {
    if (buffer == 0) {
        return -1;
    }
    if (!active) {
        return 0;
    }

    for (uint32_t index = 0; index < size; index++) {
        fbcon_put_char(buffer[index]);
    }
    return (int)size;
}