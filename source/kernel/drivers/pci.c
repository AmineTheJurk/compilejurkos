#include "../include/pci.h"
#include "../include/io.h"
#include "../include/serial.h"

#define MAX_PCI_DEVICES 64
static struct pci_device pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, val);
}

static void pci_check_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t reg0 = pci_read_config(bus, slot, func, 0x00);
    uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);
    uint16_t device_id = (uint16_t)((reg0 >> 16) & 0xFFFF);

    if (vendor_id == 0xFFFF || pci_device_count >= MAX_PCI_DEVICES) return;

    uint32_t reg8 = pci_read_config(bus, slot, func, 0x08);
    uint8_t class_id = (uint8_t)((reg8 >> 24) & 0xFF);
    uint8_t subclass = (uint8_t)((reg8 >> 16) & 0xFF);
    uint8_t prog_if  = (uint8_t)((reg8 >> 8) & 0xFF);

    uint32_t reg3c = pci_read_config(bus, slot, func, 0x3C);
    uint8_t irq = (uint8_t)(reg3c & 0xFF);

    struct pci_device *dev = &pci_devices[pci_device_count++];
    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
    dev->vendor_id = vendor_id;
    dev->device_id = device_id;
    dev->class_id = class_id;
    dev->subclass = subclass;
    dev->prog_if = prog_if;
    dev->irq = irq;

    for (int bar_idx = 0; bar_idx < 6; bar_idx++) {
        dev->bar[bar_idx] = pci_read_config(bus, slot, func, 0x10 + (bar_idx * 4));
    }

    serial_printf("PCI: Found [%x:%x] class %d:%d at %d:%d.%d\n",
                  vendor_id, device_id, class_id, subclass, bus, slot, func);
}

void pci_scan(void) {
    pci_device_count = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t reg0 = pci_read_config((uint8_t)bus, slot, 0, 0x00);
            if ((uint16_t)(reg0 & 0xFFFF) != 0xFFFF) {
                pci_check_function((uint8_t)bus, slot, 0);

                // Check for multi-function device
                uint32_t regc = pci_read_config((uint8_t)bus, slot, 0, 0x0C);
                if ((regc >> 16) & 0x80) {
                    for (uint8_t func = 1; func < 8; func++) {
                        uint32_t fn_reg0 = pci_read_config((uint8_t)bus, slot, func, 0x00);
                        if ((uint16_t)(fn_reg0 & 0xFFFF) != 0xFFFF) {
                            pci_check_function((uint8_t)bus, slot, func);
                        }
                    }
                }
            }
        }
    }
}

struct pci_device *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

struct pci_device *pci_get_device(uint32_t index) {
    if (index < pci_device_count) {
        return &pci_devices[index];
    }
    return NULL;
}

uint32_t pci_get_device_count(void) {
    return pci_device_count;
}
