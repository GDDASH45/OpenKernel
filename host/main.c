#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *qemu = getenv("OPENKERNEL_QEMU");
    const char *iso = getenv("OPENKERNEL_ISO");
    char **qemu_argv;

    if (qemu == 0 || *qemu == '\0') {
        qemu = "qemu-system-x86_64";
    }
    if (iso == 0 || *iso == '\0') {
        iso = "os.iso";
    }
    if (access(iso, R_OK) != 0) {
        fprintf(stderr, "OpenKernel boot image not found: %s\n", iso);
        fprintf(stderr, "Build and launch it with `make run-linux`.\n");
        return 1;
    }

    /* Defaults plus caller-provided QEMU options and the user's arguments. */
    qemu_argv = calloc((size_t)argc + 3, sizeof(*qemu_argv));
    if (qemu_argv == 0) {
        perror("calloc");
        return 1;
    }
    qemu_argv[0] = (char *)qemu;
    qemu_argv[1] = "-cdrom";
    qemu_argv[2] = (char *)iso;
    for (int index = 1; index < argc; index++) {
        qemu_argv[index + 2] = argv[index];
    }

    execvp(qemu, qemu_argv);
    perror("Could not start QEMU");
    free(qemu_argv);
    return 127;
}
