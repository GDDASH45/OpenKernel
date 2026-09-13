#include <assert.h>
#include <write/write.h>

static void int_to_str_basic(int n, char *buf) {
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return;
    }
    char temp[16];
    int t = 0;
    while (n > 0) {
        temp[t++] = (n % 10) + '0';
        n /= 10;
    }
    while (t > 0) {
        buf[i++] = temp[--t];
    }
    buf[i] = '\0';
}

void __assert_failed(const char *file, int line, const char *expr) {
    char line_buf[16];
    int_to_str_basic(line, line_buf);

    k_print("\n[PANIC] Assertion failed: ");
    k_print(expr);
    k_print("\n  File: ");
    k_print(file);
    k_print("\n  Line: ");
    k_print(line_buf);
    k_print("\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void __warn_triggered(const char *file, int line, const char *msg) {
    char line_buf[16];
    int_to_str_basic(line, line_buf);

    k_print("\n[WARNING] ");
    k_print(msg);
    k_print("\n  File: ");
    k_print(file);
    k_print("\n  Line: ");
    k_print(line_buf);
    k_print("\n");
}