#include <kernel/module.h>

static int __init sample_init(void) {
    // Module initialization logic goes here
    return 0;
}

static void __exit sample_exit(void) {
    // Module cleanup logic goes here
}

KERNEL_MODULE(sample, sample_init, sample_exit);