# Userspace init and shell

The active shell source is `bionicbox/shell.c`; `bionicbox/init.c` creates
runtime device nodes, performs a keyboard input check, and attempts to execute
`/shell.okx`. The kernel's default init archive entry is `sys/init`, a
prebuilt binary in the checked-in initramfs. Thus, the C files are source for
userspace programs, while the booted image is selected from archive entries.

## Shell built-ins

The shell provides:

`help`, `about`, `ls [path]`, `cd [path]`, `pwd`, `clear`, `version`, `uname`,
`whoami`, `pid`, `echo TEXT`, `true`, `false`, `status`, and `exit`.

`ls` and `cd` use the VFS directory-list syscall. The shell maintains its own
current-directory string, initially `/`; external bare names are resolved
relative to that path. This is not a process-wide kernel working directory and
there is no PATH search.

Commands not recognized as built-ins are sent to `execve()`. A successful
execution replaces the current flow until the program returns; the shell then
continues. `argv` reaches the current wrapper but the kernel loader ignores it.

## Input and parsing limits

Input is dispatched one keyboard character at a time using the userspace
`ok_input_dispatch()` helper. The parser splits on spaces only: quotes,
escapes, pipes, redirection, environment variables, and command chaining are
not implemented. Command and path buffers are fixed-size. Shell external
program execution accepts a limited number of whitespace-separated arguments.

`exit` intentionally refuses to exit PID 1. Keyboard input is translated by
the PS/2 driver; see [Display and input](../kernel/display-input.md). Directory
listing depends on the VFS tree and does not expose the separate persistent
OKFS store; see [VFS and storage](../kernel/storage.md).
