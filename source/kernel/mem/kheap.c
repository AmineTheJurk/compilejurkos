#include "../include/kheap.h"
#include "../include/string.h"

struct heap_block {
    size_t size;
    bool is_free;
    struct heap_block *next;
};

#define BLOCK_HEADER_SIZE sizeof(struct heap_block)

static struct heap_block *heap_head = NULL;

void kheap_init(uintptr_t start, size_t initial_size) {
    heap_head = (struct heap_block*)start;
    heap_head->size = initial_size - BLOCK_HEADER_SIZE;
    heap_head->is_free = true;
    heap_head->next = NULL;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 16 bytes
    size = (size + 15) & ~15;

    struct heap_block *curr = heap_head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Can we split?
            if (curr->size >= size + BLOCK_HEADER_SIZE + 16) {
                struct heap_block *new_block = (struct heap_block*)((uintptr_t)curr + BLOCK_HEADER_SIZE + size);
                new_block->size = curr->size - size - BLOCK_HEADER_SIZE;
                new_block->is_free = true;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }
            curr->is_free = false;
            return (void*)((uintptr_t)curr + BLOCK_HEADER_SIZE);
        }
        curr = curr->next;
    }
    return NULL;
}

void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void kfree(void *ptr) {
    if (!ptr) return;

    struct heap_block *block = (struct heap_block*)((uintptr_t)ptr - BLOCK_HEADER_SIZE);
    block->is_free = true;

    // Coalesce adjacent free blocks
    struct heap_block *curr = heap_head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += BLOCK_HEADER_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}
