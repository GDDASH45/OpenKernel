# To cause the kernel to panic
the api is 
void panic(const char *message);

to create a panic. call this function in some kernel code:
// For example we will use the message EXAMPLE
panic("EXAMPLE");

The kernel should then do:

KERNEL PANIC!!
EXAMPLE

END KERNEL PANIC -- EXAMPLE
EXAMPLE

You also will need to include 
#include <kernel.h>