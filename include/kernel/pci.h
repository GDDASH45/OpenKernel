#ifndef KERNEL_PCI_H
#define KERNEL_PCI_H

#include <stdint.h>

/* Enumerate PCI functions using the legacy configuration mechanism #1. */
int pci_init(void);

/* Number of PCI functions discovered by pci_init(). */
uint32_t pci_device_count(void);

#endif
