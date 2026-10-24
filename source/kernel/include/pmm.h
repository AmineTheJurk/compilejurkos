#ifndef JURKOS_PMM_H
#define JURKOS_PMM_H

#include "types.h"

#define PAGE_SIZE 4096

void pmm_init(uintptr_t mem_size, uintptr_t bitmap_base);
void pmm_mark_region(uintptr_t base, size_t length, bool used);
void *pmm_alloc_page(void);
void *pmm_alloc_pages(size_t count);
void pmm_free_page(void *ptr);
void pmm_free_pages(void *ptr, size_t count);
size_t pmm_get_free_memory(void);
size_t pmm_get_total_memory(void);

#endif
