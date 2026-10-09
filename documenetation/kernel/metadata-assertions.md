# Metadata, assertions, and warnings

## Build-time metadata

`<ksys/system-info.h>` defines compile-time constants for the codename, release
label, and repository URL. `<ksys/version.h>` defines `K_VER`. Separate
strings in `kernel/data.c` and the userspace shell contain kernel names,
banners, prompts, help text, and version values. These values are not all
wired to one authoritative version source and may disagree; do not use a
legacy banner or help string as proof that a feature is implemented.

## Assertions

`<assert.h>` provides `assert(expression)` unless `NDEBUG` is defined and
`WARN(expression, message)` for nonfatal reports. A failed assertion prints
the expression, source file, and line, then halts. A failed `WARN` prints its
message, file, and line and returns to the caller. The handlers are in
`kernel/assert.c`; assertion failure has its own halt path rather than using
the regular panic register dump. See [Interrupts and panic handling](interrupts.md).

## Legacy user record

`<user/user.h>` declares a `user_process_t` record and
`user_init()`/`user_create_process()` functions implemented in `kernel/user.c`.
That code only updates one static record and logs messages. It is not the
loader/process mechanism used by `execve()` and does not create an isolated or
scheduled userspace task. See [Programs and process execution](processes.md).
