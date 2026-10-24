#ifndef JURKOS_PCI_H
#define JURKOS_PCI_H

#include "types.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

struct pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_id;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  irq;
    uint32_t bar[6];
};

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void pci_scan(void);
struct pci_device *pci_find_device(uint16_t vendor_id, uint16_t device_id);
struct pci_device *pci_get_device(uint32_t index);
uint32_t pci_get_device_count(void);

#endif
