#include "../include/pmm.h"
#include "../include/string.h"

static uint8_t *bitmap = NULL;
static size_t total_pages = 0;
static size_t free_pages = 0;

static inline void bitmap_set(size_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(size_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool bitmap_test(size_t bit) {
    return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

void pmm_init(uintptr_t mem_size, uintptr_t bitmap_base) {
    total_pages = mem_size / PAGE_SIZE;
    size_t bitmap_size = (total_pages + 7) / 8;
    bitmap = (uint8_t*)bitmap_base;

    // Mark all as used initially
    memset(bitmap, 0xFF, bitmap_size);
    free_pages = 0;
}

void pmm_mark_region(uintptr_t base, size_t length, bool used) {
    size_t start_page = base / PAGE_SIZE;
    size_t page_count = length / PAGE_SIZE;

    for (size_t i = 0; i < page_count && (start_page + i) < total_pages; i++) {
        size_t page = start_page + i;
        if (used) {
            if (!bitmap_test(page)) {
                bitmap_set(page);
                if (free_pages > 0) free_pages--;
            }
        } else {
            if (bitmap_test(page)) {
                bitmap_clear(page);
                free_pages++;
            }
        }
    }
}

void *pmm_alloc_page(void) {
    for (size_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            return (void*)(uintptr_t)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void *pmm_alloc_pages(size_t count) {
    if (count == 0) return NULL;
    size_t consecutive = 0;
    size_t start_page = 0;

    for (size_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) start_page = i;
            consecutive++;
            if (consecutive == count) {
                for (size_t j = 0; j < count; j++) {
                    bitmap_set(start_page + j);
                }
                free_pages -= count;
                return (void*)(uintptr_t)(start_page * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    return NULL;
}

void pmm_free_page(void *ptr) {
    if (!ptr) return;
    size_t page = ((uintptr_t)ptr) / PAGE_SIZE;
    if (page < total_pages && bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}

void pmm_free_pages(void *ptr, size_t count) {
    if (!ptr) return;
    size_t start_page = ((uintptr_t)ptr) / PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        size_t page = start_page + i;
        if (page < total_pages && bitmap_test(page)) {
            bitmap_clear(page);
            free_pages++;
        }
    }
}

size_t pmm_get_free_memory(void) {
    return free_pages * PAGE_SIZE;
}

size_t pmm_get_total_memory(void) {
    return total_pages * PAGE_SIZE;
}
