#ifndef JURKOS_IDT_H
#define JURKOS_IDT_H

#include "types.h"

#if defined(__x86_64__)
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));
#else
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));
#endif

struct idt_ptr {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed));

struct interrupt_frame {
    uintptr_t ip;
    uintptr_t cs;
    uintptr_t flags;
    uintptr_t sp;
    uintptr_t ss;
};

void idt_init(void);
void idt_set_gate(uint8_t num, uintptr_t base, uint16_t sel, uint8_t flags);
void register_interrupt_handler(uint8_t n, void (*handler)(struct interrupt_frame *frame));

#endif
