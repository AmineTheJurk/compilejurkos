#include "../include/loader.h"
#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/serial.h"

#define XEC_BASE_VADDR 0x40000000ULL

int xec_load(const uint8_t *data, size_t size, uintptr_t *entry_out) {
    if (!data || size == 0) return -1;

    size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    serial_printf("XEC: Loading flat binary of %d bytes (%d pages) at 0x%x\n",
                  (int)size, (int)pages_needed, XEC_BASE_VADDR);

    for (size_t i = 0; i < pages_needed; i++) {
        uintptr_t virt = XEC_BASE_VADDR + (i * PAGE_SIZE);
        void *phys = pmm_alloc_page();
        memset(phys, 0, PAGE_SIZE);
        vmm_map_page((uintptr_t)phys, virt, PAGE_USER | PAGE_WRITABLE);
    }

    memcpy((void*)XEC_BASE_VADDR, data, size);
    *entry_out = XEC_BASE_VADDR;
    return 0;
}
