#include <kernel/bootopts.h>

#define INIT_PATH_CAPACITY 128
#define CONSOLE_PATH_CAPACITY 32

static char init_path[INIT_PATH_CAPACITY] = "sys/init";
static char console_path[CONSOLE_PATH_CAPACITY] = "/device/console";
static int quiet_mode;

static int token_equal(const char *token, uint32_t length, const char *expected) {
    uint32_t index = 0;

    while (expected[index] != '\0' && index < length &&
           token[index] == expected[index]) {
        index++;
    }
    return index == length && expected[index] == '\0';
}

static int copy_option(char *destination, uint32_t capacity,
                       const char *source, uint32_t size) {
    if (size == 0 || size >= capacity) {
        return 0;
    }
    for (uint32_t index = 0; index < size; index++) {
        destination[index] = source[index];
    }
    destination[size] = '\0';
    return 1;
}

void bootopts_init(const char *command_line) {
    uint32_t index = 0;

    quiet_mode = 0;
    copy_option(init_path, sizeof(init_path), "sys/init", 8);
    console_path[0] = '\0';
    copy_option(console_path, sizeof(console_path),
                "/device/console", 15);

    if (command_line == 0) {
        return;
    }

    while (command_line[index] != '\0') {
        uint32_t start;
        uint32_t length;

        while (command_line[index] == ' ' || command_line[index] == '\t') {
            index++;
        }
        start = index;
        while (command_line[index] != '\0' && command_line[index] != ' ' &&
               command_line[index] != '\t') {
            index++;
        }
        length = index - start;
        if (length == 0) {
            continue;
        }

        if (token_equal(command_line + start, length, "quiet")) {
            quiet_mode = 1;
        } else if (length > 5 &&
                   command_line[start] == 'i' && command_line[start + 1] == 'n' &&
                   command_line[start + 2] == 'i' && command_line[start + 3] == 't' &&
                   command_line[start + 4] == '=') {
            (void)copy_option(init_path, sizeof(init_path),
                              command_line + start + 5, length - 5);
        } else if (length > 8 &&
                   command_line[start] == 'c' && command_line[start + 1] == 'o' &&
                   command_line[start + 2] == 'n' && command_line[start + 3] == 's' &&
                   command_line[start + 4] == 'o' && command_line[start + 5] == 'l' &&
                   command_line[start + 6] == 'e' && command_line[start + 7] == '=') {
            (void)copy_option(console_path, sizeof(console_path),
                              command_line + start + 8, length - 8);
        }
    }
}

int bootopts_quiet(void) {
    return quiet_mode;
}

const char *bootopts_init_path(void) {
    return init_path;
}

const char *bootopts_console_path(void) {
    return console_path;
}
