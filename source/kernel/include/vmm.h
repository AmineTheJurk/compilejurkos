#ifndef JURKOS_VMM_H
#define JURKOS_VMM_H

#include "types.h"

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

typedef uintptr_t page_directory_t;

void vmm_init(void);
void vmm_map_page(uintptr_t phys, uintptr_t virt, uint64_t flags);
void vmm_unmap_page(uintptr_t virt);
uintptr_t vmm_get_phys(uintptr_t virt);
void vmm_switch_pml4(uintptr_t pml4_phys);

#endif
