#include "../../include/gdt.h"
#include "../../include/string.h"

static struct gdt_entry gdt_entries[5];
static struct gdt_ptr   gdt_pointer;

extern void gdt_flush(uintptr_t);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt_entries[num].access      = access;
}

void gdt_init(void) {
    gdt_pointer.limit = (sizeof(struct gdt_entry) * 5) - 1;
    gdt_pointer.base  = (uintptr_t)&gdt_entries;

    // 0x00: Null segment
    gdt_set_gate(0, 0, 0, 0, 0);

    // 0x08: Kernel 64-bit Code (Execute/Read, DPL 0, Long mode flag bit 53)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0x20);

    // 0x10: Kernel 64-bit Data (Read/Write, DPL 0)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0x00);

    // 0x18: User 64-bit Data (Read/Write, DPL 3)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xF2, 0x00);

    // 0x20: User 64-bit Code (Execute/Read, DPL 3, Long mode flag bit 53)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xFA, 0x20);

    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        : : "m"(gdt_pointer) : "rax", "memory"
    );
}
