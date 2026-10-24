#ifndef JURKOS_E1000_H
#define JURKOS_E1000_H

#include "types.h"
#include "pci.h"

#define E1000_VENDOR_INTEL 0x8086
#define E1000_DEV_82540EM  0x100E
#define E1000_DEV_82545EM  0x100F

bool e1000_init(struct pci_device *pci_dev);
int e1000_send_packet(const uint8_t *packet, uint16_t length);
int e1000_receive_packet(uint8_t *buffer, uint16_t max_length);
const uint8_t *e1000_get_mac(void);

#endif
