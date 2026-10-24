#include "../../include/vmm.h"
#include "../../include/pmm.h"
#include "../../include/string.h"

static uint32_t *kernel_page_dir = NULL;

void vmm_init(void) {
    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    kernel_page_dir = (uint32_t*)(uintptr_t)(cr3 & ~0xFFF);
}

void vmm_map_page(uintptr_t phys, uintptr_t virt, uint64_t flags) {
    size_t pd_idx = (virt >> 22) & 0x3FF;
    size_t pt_idx = (virt >> 12) & 0x3FF;

    if (!kernel_page_dir) {
        vmm_init();
    }

    uint32_t *pt;
    if (!(kernel_page_dir[pd_idx] & PAGE_PRESENT)) {
        pt = (uint32_t*)pmm_alloc_page();
        memset(pt, 0, PAGE_SIZE);
        kernel_page_dir[pd_idx] = (uint32_t)(uintptr_t)pt | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    } else {
        pt = (uint32_t*)(uintptr_t)(kernel_page_dir[pd_idx] & ~0xFFF);
    }

    pt[pt_idx] = (phys & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap_page(uintptr_t virt) {
    size_t pd_idx = (virt >> 22) & 0x3FF;
    size_t pt_idx = (virt >> 12) & 0x3FF;

    if (!kernel_page_dir || !(kernel_page_dir[pd_idx] & PAGE_PRESENT)) return;
    uint32_t *pt = (uint32_t*)(uintptr_t)(kernel_page_dir[pd_idx] & ~0xFFF);

    pt[pt_idx] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uintptr_t vmm_get_phys(uintptr_t virt) {
    size_t pd_idx = (virt >> 22) & 0x3FF;
    size_t pt_idx = (virt >> 12) & 0x3FF;

    if (!kernel_page_dir || !(kernel_page_dir[pd_idx] & PAGE_PRESENT)) return 0;
    uint32_t *pt = (uint32_t*)(uintptr_t)(kernel_page_dir[pd_idx] & ~0xFFF);

    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    return (pt[pt_idx] & ~0xFFF) | (virt & 0xFFF);
}

void vmm_switch_pml4(uintptr_t pml4_phys) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)pml4_phys) : "memory");
}
