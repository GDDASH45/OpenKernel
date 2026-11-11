#ifndef SDRIVERS_NET_VIRTIO_VIRTIO_NET_H
#define SDRIVERS_NET_VIRTIO_VIRTIO_NET_H

#include <stdint.h>

#define VIRTIO_NET_FRAME_MAX 1518u

int virtio_net_init(uint8_t mac_address[6]);
int virtio_net_send(const void *frame, uint16_t length);
int virtio_net_receive(void *frame, uint16_t capacity);

#endif
