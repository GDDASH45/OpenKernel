#include "virtio_net.h"
#include <kernel/ports.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define VIRTIO_VENDOR_ID 0x1AF4u
#define VIRTIO_NET_DEVICE_ID 0x1000u

#define VIRTIO_PCI_DEVICE_FEATURES 0
#define VIRTIO_PCI_GUEST_FEATURES 4
#define VIRTIO_PCI_QUEUE_ADDRESS 8
#define VIRTIO_PCI_QUEUE_SIZE 12
#define VIRTIO_PCI_QUEUE_SELECT 14
#define VIRTIO_PCI_QUEUE_NOTIFY 16
#define VIRTIO_PCI_STATUS 18
#define VIRTIO_PCI_CONFIG 20

#define VIRTIO_STATUS_ACKNOWLEDGE 1u
#define VIRTIO_STATUS_DRIVER 2u
#define VIRTIO_STATUS_DRIVER_OK 4u
#define VIRTIO_STATUS_FEATURES_OK 8u
#define VIRTIO_STATUS_FAILED 128u
#define VIRTIO_NET_F_MAC (1u << 5)

#define VIRTIO_NET_QUEUE_SIZE 256u
#define VIRTIO_NET_BUFFER_SIZE 2048u
#define VIRTIO_NET_HEADER_SIZE 10u
#define VIRTIO_DESC_F_WRITE 2u

struct virtio_descriptor {
	uint64_t address;
	uint32_t length;
	uint16_t flags;
	uint16_t next;
} __attribute__((packed));

struct virtio_available {
	uint16_t flags;
	volatile uint16_t index;
	uint16_t ring[VIRTIO_NET_QUEUE_SIZE];
} __attribute__((packed));

struct virtio_used_element {
	uint32_t id;
	uint32_t length;
} __attribute__((packed));

struct virtio_used {
	uint16_t flags;
	volatile uint16_t index;
	struct virtio_used_element ring[VIRTIO_NET_QUEUE_SIZE];
} __attribute__((packed));

struct virtio_queue {
	struct virtio_descriptor descriptors[VIRTIO_NET_QUEUE_SIZE];
	struct virtio_available available;
	uint8_t alignment_padding[3580];
	struct virtio_used used;
} __attribute__((packed));

typedef char virtio_queue_layout_must_fit[
	(sizeof(struct virtio_queue) == 10244) ? 1 : -1];

static struct virtio_queue rx_queue __attribute__((aligned(4096)));
static struct virtio_queue tx_queue __attribute__((aligned(4096)));
static uint8_t rx_buffers[VIRTIO_NET_QUEUE_SIZE][VIRTIO_NET_BUFFER_SIZE]
	__attribute__((aligned(16)));
static uint8_t tx_buffers[VIRTIO_NET_QUEUE_SIZE][VIRTIO_NET_BUFFER_SIZE]
	__attribute__((aligned(16)));
static uint8_t tx_free[VIRTIO_NET_QUEUE_SIZE];
static uint16_t rx_last_used;
static uint16_t tx_last_used;
static uint16_t tx_available_index;
static uint16_t tx_cursor;
static uint16_t queue_size;
static uint16_t io_base;
static int initialized;

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t function,
						   uint8_t offset) {
	uint32_t address = 0x80000000u | ((uint32_t)bus << 16) |
					   ((uint32_t)slot << 11) |
					   ((uint32_t)function << 8) | (offset & 0xFCu);
	uint32_t value;

	__asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
	__asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(PCI_CONFIG_DATA));
	return value;
}

static void pci_write16(uint8_t bus, uint8_t slot, uint8_t function,
						uint8_t offset, uint16_t value) {
	uint32_t address = 0x80000000u | ((uint32_t)bus << 16) |
					   ((uint32_t)slot << 11) |
					   ((uint32_t)function << 8) | (offset & 0xFCu);

	__asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
	__asm__ volatile ("outw %0, %1"
					  : : "a"(value),
						  "Nd"((uint16_t)(PCI_CONFIG_DATA + (offset & 2u))));
}

static int find_device(uint8_t *found_bus, uint8_t *found_slot,
					   uint8_t *found_function, uint32_t *found_bar) {
	for (uint16_t bus = 0; bus < 256; bus++) {
		for (uint8_t slot = 0; slot < 32; slot++) {
			for (uint8_t function = 0; function < 8; function++) {
				uint32_t id = pci_read32((uint8_t)bus, slot, function, 0);
				uint16_t vendor = (uint16_t)id;
				uint16_t device = (uint16_t)(id >> 16);

				if (vendor == 0xFFFFu) {
					if (function == 0) {
						break;
					}
					continue;
				}
				if (vendor == VIRTIO_VENDOR_ID &&
					device == VIRTIO_NET_DEVICE_ID) {
					*found_bus = (uint8_t)bus;
					*found_slot = slot;
					*found_function = function;
					*found_bar = pci_read32((uint8_t)bus, slot, function, 0x10);
					return 0;
				}
				if (function == 0 &&
					((pci_read32((uint8_t)bus, slot, 0, 0x0C) >> 16) &
					 0x80u) == 0) {
					break;
				}
			}
		}
	}
	return -1;
}

static uint8_t io_read8(uint16_t offset) {
	return port_byte_in((uint16_t)(io_base + offset));
}

static uint16_t io_read16(uint16_t offset) {
	uint16_t value;
	__asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"((uint16_t)(io_base + offset)));
	return value;
}

static uint32_t io_read32(uint16_t offset) {
	uint32_t value;
	__asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"((uint16_t)(io_base + offset)));
	return value;
}

static void io_write8(uint16_t offset, uint8_t value) {
	port_byte_out((uint16_t)(io_base + offset), value);
}

static void io_write16(uint16_t offset, uint16_t value) {
	__asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"((uint16_t)(io_base + offset)));
}

static void io_write32(uint16_t offset, uint32_t value) {
	__asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"((uint16_t)(io_base + offset)));
}

static int setup_queue(uint16_t index, struct virtio_queue *queue) {
	uint16_t size;
	uint32_t address = (uint32_t)(uintptr_t)queue;

	io_write16(VIRTIO_PCI_QUEUE_SELECT, index);
	size = io_read16(VIRTIO_PCI_QUEUE_SIZE);
	if (size != VIRTIO_NET_QUEUE_SIZE ||
		(address & 0xFFFu) != 0 ||
		io_read32(VIRTIO_PCI_QUEUE_ADDRESS) != 0) {
		return -1;
	}
	if (index == 0) {
		queue_size = size;
	} else if (size != queue_size) {
		return -1;
	}
	__asm__ volatile ("" : : : "memory");
	io_write32(VIRTIO_PCI_QUEUE_ADDRESS, address >> 12);
	return 0;
}

static void reclaim_tx(void) {
	__asm__ volatile ("" : : : "memory");
	while (tx_last_used != tx_queue.used.index) {
		__asm__ volatile ("" : : : "memory");
		uint16_t used_slot = tx_last_used % queue_size;
		uint32_t descriptor = tx_queue.used.ring[used_slot].id;

		if (descriptor < queue_size) {
			tx_free[descriptor] = 1;
		} else {
			initialized = 0;
			io_write8(VIRTIO_PCI_STATUS, VIRTIO_STATUS_FAILED);
			return;
		}
		tx_last_used++;
	}
}

int virtio_net_init(uint8_t mac_address[6]) {
	uint8_t bus;
	uint8_t slot;
	uint8_t function;
	uint32_t bar;
	uint32_t device_features;
	uint16_t command;
	uint8_t status;

	if (mac_address == 0 || initialized) {
		return initialized ? 0 : -1;
	}
	if (find_device(&bus, &slot, &function, &bar) != 0 ||
		(bar & 1u) == 0 || (bar & 0xFFFFFFFCu) == 0 ||
		(bar & 0xFFFF0000u) != 0) {
		return -1;
	}

	io_base = (uint16_t)(bar & 0xFFFCu);
	command = (uint16_t)pci_read32(bus, slot, function, 0x04);
	command |= 0x0005u;
	pci_write16(bus, slot, function, 0x04, command);

	status = 0;
	io_write8(VIRTIO_PCI_STATUS, status);
	status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
	io_write8(VIRTIO_PCI_STATUS, status);

	device_features = io_read32(VIRTIO_PCI_DEVICE_FEATURES);
	if ((device_features & VIRTIO_NET_F_MAC) == 0) {
		io_write8(VIRTIO_PCI_STATUS,
				  (uint8_t)(status | VIRTIO_STATUS_FAILED));
		return -1;
	}
	io_write32(VIRTIO_PCI_GUEST_FEATURES, VIRTIO_NET_F_MAC);
	status |= VIRTIO_STATUS_FEATURES_OK;
	io_write8(VIRTIO_PCI_STATUS, status);
	if ((io_read8(VIRTIO_PCI_STATUS) & VIRTIO_STATUS_FEATURES_OK) == 0) {
		io_write8(VIRTIO_PCI_STATUS,
				  (uint8_t)(status | VIRTIO_STATUS_FAILED));
		return -1;
	}

	for (uint32_t index = 0; index < 6; index++) {
		mac_address[index] = io_read8((uint16_t)(VIRTIO_PCI_CONFIG + index));
	}

	for (uint32_t index = 0; index < sizeof(rx_queue); index++) {
		((uint8_t *)&rx_queue)[index] = 0;
		((uint8_t *)&tx_queue)[index] = 0;
	}
	for (uint32_t index = 0; index < VIRTIO_NET_QUEUE_SIZE; index++) {
		tx_free[index] = 1;
		rx_queue.descriptors[index].address =
			(uint32_t)(uintptr_t)rx_buffers[index];
		rx_queue.descriptors[index].length = VIRTIO_NET_BUFFER_SIZE;
		rx_queue.descriptors[index].flags = VIRTIO_DESC_F_WRITE;
		rx_queue.available.ring[index] = (uint16_t)index;
	}
	rx_queue.available.index = VIRTIO_NET_QUEUE_SIZE;
	rx_last_used = 0;
	tx_last_used = 0;
	tx_available_index = 0;
	tx_cursor = 0;

	if (setup_queue(0, &rx_queue) != 0 ||
		setup_queue(1, &tx_queue) != 0) {
		io_write8(VIRTIO_PCI_STATUS,
				  (uint8_t)(status | VIRTIO_STATUS_FAILED));
		return -1;
	}

	status |= VIRTIO_STATUS_DRIVER_OK;
	__asm__ volatile ("" : : : "memory");
	io_write8(VIRTIO_PCI_STATUS, status);
	initialized = 1;
	return 0;
}

int virtio_net_send(const void *frame, uint16_t length) {
	uint16_t descriptor = queue_size;
	uint32_t timeout = 1000000;

	if (!initialized || frame == 0 || length < 14 ||
		length > VIRTIO_NET_FRAME_MAX) {
		return -1;
	}
	reclaim_tx();
	if (!initialized) {
		return -1;
	}
	while (descriptor == queue_size && timeout-- != 0) {
		for (uint16_t offset = 0; offset < queue_size; offset++) {
			uint16_t candidate = (uint16_t)((tx_cursor + offset) % queue_size);
			if (tx_free[candidate]) {
				descriptor = candidate;
				tx_cursor = (uint16_t)((candidate + 1) % queue_size);
				break;
			}
		}
		if (descriptor == queue_size) {
			reclaim_tx();
			if (!initialized) {
				return -1;
			}
			__asm__ volatile ("pause");
		}
	}
	if (descriptor == queue_size) {
		return -1;
	}

	for (uint32_t index = 0; index < VIRTIO_NET_HEADER_SIZE; index++) {
		tx_buffers[descriptor][index] = 0;
	}
	for (uint32_t index = 0; index < length; index++) {
		tx_buffers[descriptor][VIRTIO_NET_HEADER_SIZE + index] =
			((const uint8_t *)frame)[index];
	}
	tx_queue.descriptors[descriptor].address =
		(uint32_t)(uintptr_t)tx_buffers[descriptor];
	tx_queue.descriptors[descriptor].length =
		(uint32_t)length + VIRTIO_NET_HEADER_SIZE;
	tx_queue.descriptors[descriptor].flags = 0;
	tx_queue.descriptors[descriptor].next = 0;
	tx_free[descriptor] = 0;
	tx_queue.available.ring[tx_available_index % queue_size] = descriptor;
	__asm__ volatile ("" : : : "memory");
	tx_available_index++;
	tx_queue.available.index = tx_available_index;
	__asm__ volatile ("" : : : "memory");
	io_write16(VIRTIO_PCI_QUEUE_NOTIFY, 1);
	return (int)length;
}

int virtio_net_receive(void *frame, uint16_t capacity) {
	if (!initialized || frame == 0) {
		return -1;
	}

	__asm__ volatile ("" : : : "memory");
	if (rx_last_used == rx_queue.used.index) {
		return 0;
	}
	__asm__ volatile ("" : : : "memory");

	while (rx_last_used != rx_queue.used.index) {
		__asm__ volatile ("" : : : "memory");
		uint16_t used_slot = rx_last_used % queue_size;
		uint32_t descriptor = rx_queue.used.ring[used_slot].id;
		uint32_t used_length = rx_queue.used.ring[used_slot].length;
		uint32_t frame_length = 0;
		int result = 0;

		rx_last_used++;
		if (descriptor >= queue_size) {
			initialized = 0;
			io_write8(VIRTIO_PCI_STATUS, VIRTIO_STATUS_FAILED);
			return -1;
		}
		if (used_length >= VIRTIO_NET_HEADER_SIZE &&
			used_length <= VIRTIO_NET_BUFFER_SIZE) {
			frame_length = used_length - VIRTIO_NET_HEADER_SIZE;
			if (frame_length >= 14 && frame_length <= capacity &&
				frame_length <= VIRTIO_NET_FRAME_MAX) {
				for (uint32_t index = 0; index < frame_length; index++) {
					((uint8_t *)frame)[index] =
						rx_buffers[descriptor][VIRTIO_NET_HEADER_SIZE + index];
				}
				result = (int)frame_length;
			}
		}
		rx_queue.available.ring[
			rx_queue.available.index % queue_size] = (uint16_t)descriptor;
		__asm__ volatile ("" : : : "memory");
		rx_queue.available.index++;
		__asm__ volatile ("" : : : "memory");
		io_write16(VIRTIO_PCI_QUEUE_NOTIFY, 0);
		if (result != 0) {
			return result;
		}
	}
	return 0;
}
