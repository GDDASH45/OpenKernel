#include <kernel/script.h>
#include <write/write.h>
#include <kernel/exec.h>

// Simple string comparison helper
static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*prefix++ != *str++) return 0;
    }
    return 1;
}

static int str_equals(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 == *(const unsigned char*)s2;
}

int script_run(const char *script_data, uint32_t size, uint32_t initrd_start) {
    int command_found = 0;

    if (script_data == 0 || size == 0) {
        return -1;
    }
    k_print("Running startup script...\n");

    const char *ptr = script_data;
    const char *end = script_data + size;

    char line_buf[128];
    uint32_t line_idx = 0;

    while (ptr < end) {
        char c = *ptr++;

        if (c == '\r') {
            if (ptr < end) {
                continue;
            }
            c = '\n';
        }
        if (c == '\n' || ptr == end) {
            line_buf[line_idx] = '\0';

            // Parse line if not empty or comment
            if (line_idx > 0 && line_buf[0] != '#') {
                if (str_equals(line_buf, "clear")) {
                    k_clear_screen();
                    command_found = 1;
                } else if (str_starts_with(line_buf, "print ")) {
                    // Print text after "print "
                    k_print(line_buf + 6);
                    k_print("\n");
                    command_found = 1;
                } else if (str_starts_with(line_buf, "exec ")) {
                    if (line_buf[5] == '\0' ||
                        run_binary(initrd_start, line_buf + 5) != 0) {
                        return -1;
                    }
                    command_found = 1;
                } else {
                    k_print("Unknown init command: ");
                    k_print(line_buf);
                    k_print("\n");
                    return -1;
                }
            }

            line_idx = 0;
        } else if (line_idx < sizeof(line_buf) - 1) {
            line_buf[line_idx++] = c;
        } else {
            k_print("Init script line is too long.\n");
            return -1;
        }
    }

    return command_found ? 0 : -1;
}