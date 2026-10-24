#ifndef JURKOS_KHEAP_H
#define JURKOS_KHEAP_H

#include "types.h"

void kheap_init(uintptr_t start, size_t initial_size);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(void *ptr);

#endif
