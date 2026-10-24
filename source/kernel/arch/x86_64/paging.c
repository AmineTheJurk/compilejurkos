#include "../../include/vmm.h"
#include "../../include/pmm.h"
#include "../../include/string.h"

static uint64_t *kernel_pml4 = NULL;

void vmm_init(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    kernel_pml4 = (uint64_t*)(uintptr_t)(cr3 & ~0xFFFULL);
}

void vmm_map_page(uintptr_t phys, uintptr_t virt, uint64_t flags) {
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!kernel_pml4) {
        vmm_init();
    }

    uint64_t *pdpt;
    if (!(kernel_pml4[pml4_idx] & PAGE_PRESENT)) {
        pdpt = (uint64_t*)pmm_alloc_page();
        memset(pdpt, 0, PAGE_SIZE);
        kernel_pml4[pml4_idx] = (uintptr_t)pdpt | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    } else {
        pdpt = (uint64_t*)(uintptr_t)(kernel_pml4[pml4_idx] & ~0xFFFULL);
    }

    uint64_t *pd;
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        pd = (uint64_t*)pmm_alloc_page();
        memset(pd, 0, PAGE_SIZE);
        pdpt[pdpt_idx] = (uintptr_t)pd | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    } else {
        pd = (uint64_t*)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);
    }

    uint64_t *pt;
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        pt = (uint64_t*)pmm_alloc_page();
        memset(pt, 0, PAGE_SIZE);
        pd[pd_idx] = (uintptr_t)pt | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    } else {
        pt = (uint64_t*)(uintptr_t)(pd[pd_idx] & ~0xFFFULL);
    }

    pt[pt_idx] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;

    // Invalidate TLB for virtual address
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap_page(uintptr_t virt) {
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!kernel_pml4 || !(kernel_pml4[pml4_idx] & PAGE_PRESENT)) return;
    uint64_t *pdpt = (uint64_t*)(uintptr_t)(kernel_pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return;
    uint64_t *pd = (uint64_t*)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PAGE_PRESENT)) return;
    uint64_t *pt = (uint64_t*)(uintptr_t)(pd[pd_idx] & ~0xFFFULL);

    pt[pt_idx] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uintptr_t vmm_get_phys(uintptr_t virt) {
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!kernel_pml4 || !(kernel_pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    uint64_t *pdpt = (uint64_t*)(uintptr_t)(kernel_pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    uint64_t *pd = (uint64_t*)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    uint64_t *pt = (uint64_t*)(uintptr_t)(pd[pd_idx] & ~0xFFFULL);

    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    return (pt[pt_idx] & ~0xFFFULL) | (virt & 0xFFFULL);
}

void vmm_switch_pml4(uintptr_t pml4_phys) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}
