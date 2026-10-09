# Kernel module status

`<kernel/module.h>` defines a `kernel_module_t` descriptor, `KERNEL_MODULE()`
registration macro, and `__init` / `__exit` section attributes. The current
build compiles files in `modules/` into module object outputs.

The existing registry code can insert descriptors into a table, but there is
no demonstrated archive/module loader, runtime scan or initialization pass,
unload path, dependency management, or lifecycle controller. Do not assume a
module is loaded merely because a `.mo` or `.mo`-like artifact was built or a
`KERNEL_MODULE()` declaration exists. `info_module_init()` is explicitly called
from startup and is separate from a general-purpose module manager.

Treat module support as experimental scaffolding until a runtime loading and
lifecycle path is implemented. Relevant sources are `include/kernel/module.h`,
`kernel/module.c`, and `modules/`.
