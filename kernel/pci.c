#include <stdint.h>
#include <kernel/pci.h>
#include <write/write.h>

#define PCI_CONFIG_ADDRESS 0x0CF8u
#define PCI_CONFIG_DATA    0x0CFcu
#define PCI_CONFIG_ENABLE  0x80000000u

static uint32_t discovered_devices;

static inline void pci_outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t pci_inl(uint16_t port)
{
    uint32_t value;

    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint32_t pci_read_config(uint8_t bus, uint8_t device,
                                uint8_t function, uint8_t offset)
{
    uint32_t address = PCI_CONFIG_ENABLE |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) |
                       ((uint32_t)offset & 0xFCu);

    pci_outl(PCI_CONFIG_ADDRESS, address);
    return pci_inl(PCI_CONFIG_DATA);
}

static void pci_report_function(uint8_t bus, uint8_t device,
                                uint8_t function)
{
    uint32_t identity = pci_read_config(bus, device, function, 0x00);
    uint32_t class_info = pci_read_config(bus, device, function, 0x08);
    uint16_t vendor = (uint16_t)identity;
    uint16_t product = (uint16_t)(identity >> 16);
    uint8_t programming_interface = (uint8_t)(class_info >> 8);
    uint8_t subclass = (uint8_t)(class_info >> 16);
    uint8_t class_code = (uint8_t)(class_info >> 24);

    if (vendor == 0xFFFFu)
        return;

    discovered_devices++;
    printk("[PCI] %x:%x.%x vendor=%x device=%x class=%x:%x prog-if=%x\n",
           (uint32_t)bus, (uint32_t)device, (uint32_t)function,
           (uint32_t)vendor, (uint32_t)product, (uint32_t)class_code,
           (uint32_t)subclass, (uint32_t)programming_interface);
}

int pci_init(void)
{
    uint32_t previous_address = pci_inl(PCI_CONFIG_ADDRESS);

    discovered_devices = 0;
    pci_outl(PCI_CONFIG_ADDRESS, PCI_CONFIG_ENABLE);

    if ((pci_inl(PCI_CONFIG_ADDRESS) & PCI_CONFIG_ENABLE) == 0) {
        pci_outl(PCI_CONFIG_ADDRESS, previous_address);
        return -1;
    }

    for (uint32_t bus = 0; bus < 256u; bus++) {
        for (uint32_t device = 0; device < 32u; device++) {
            uint32_t identity =
                pci_read_config((uint8_t)bus, (uint8_t)device, 0, 0x00);
            uint32_t header =
                pci_read_config((uint8_t)bus, (uint8_t)device, 0, 0x0C);
            uint32_t functions =
                ((header >> 16) & 0x80u) ? 8u : 1u;

            if ((uint16_t)identity == 0xFFFFu)
                continue;

            pci_report_function((uint8_t)bus, (uint8_t)device, 0);

            for (uint32_t function = 1; function < functions; function++) {
                pci_report_function((uint8_t)bus, (uint8_t)device,
                                    (uint8_t)function);
            }
        }
    }

    pci_outl(PCI_CONFIG_ADDRESS, previous_address);
    printk("[PCI] Enumeration complete: %u function(s) found.\n",
           discovered_devices);
    return 0;
}

uint32_t pci_device_count(void)
{
    return discovered_devices;
}
