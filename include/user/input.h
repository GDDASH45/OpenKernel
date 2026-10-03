#ifndef USER_INPUT_H
#define USER_INPUT_H

#include <stdint.h>
#include <user/syscall.h>

typedef void (*ok_input_handler_t)(char character, void *context);

/*
 * Dispatch queued keyboard characters to a userspace callback. The kernel
 * keyboard IRQ translates scancodes; this layer provides app-level handling.
 */
static inline int32_t ok_input_dispatch(ok_input_handler_t handler,
                                        void *context) {
    if (handler == 0) {
        return -14;
    }

    for (;;) {
        int32_t input = ok_getchar();
        if (input < 0) {
            return input;
        }
        handler((char)input, context);
    }
}

#endif
