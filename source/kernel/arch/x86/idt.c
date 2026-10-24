#include "../../include/idt.h"
#include "../../include/io.h"
#include "../../include/string.h"
#include "../../include/serial.h"

static struct idt_entry idt_entries[256];
static struct idt_ptr   idt_pointer;

extern void isr0(void); extern void isr1(void); extern void isr2(void); extern void isr3(void);
extern void isr4(void); extern void isr5(void); extern void isr6(void); extern void isr7(void);
extern void isr8(void); extern void isr9(void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void); extern void irq1(void); extern void irq2(void); extern void irq3(void);
extern void irq4(void); extern void irq5(void); extern void irq6(void); extern void irq7(void);
extern void irq8(void); extern void irq9(void); extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static void (*interrupt_handlers[256])(struct interrupt_frame *frame) = {0};

void register_interrupt_handler(uint8_t n, void (*handler)(struct interrupt_frame *frame)) {
    interrupt_handlers[n] = handler;
}

void idt_set_gate(uint8_t num, uintptr_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].offset_low  = (base & 0xFFFF);
    idt_entries[num].selector    = sel;
    idt_entries[num].zero        = 0;
    idt_entries[num].type_attr   = flags;
    idt_entries[num].offset_high = (base >> 16) & 0xFFFF;
}

static void pic_remap(void) {
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();

    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();

    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

void idt_init(void) {
    idt_pointer.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_pointer.base  = (uintptr_t)&idt_entries;

    memset(&idt_entries, 0, sizeof(struct idt_entry) * 256);
    pic_remap();

    idt_set_gate(0,  (uintptr_t)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uintptr_t)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uintptr_t)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uintptr_t)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (uintptr_t)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uintptr_t)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uintptr_t)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uintptr_t)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (uintptr_t)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (uintptr_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uintptr_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uintptr_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uintptr_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uintptr_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uintptr_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uintptr_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uintptr_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uintptr_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uintptr_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uintptr_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uintptr_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uintptr_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uintptr_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uintptr_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uintptr_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uintptr_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uintptr_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uintptr_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uintptr_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uintptr_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uintptr_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uintptr_t)isr31, 0x08, 0x8E);

    idt_set_gate(32, (uintptr_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uintptr_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uintptr_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uintptr_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uintptr_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uintptr_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uintptr_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uintptr_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uintptr_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uintptr_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uintptr_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uintptr_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uintptr_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uintptr_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uintptr_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uintptr_t)irq15, 0x08, 0x8E);

    __asm__ volatile ("lidt %0" : : "m"(idt_pointer));
    sti();
}

struct cpu_state_32 {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

void isr_handler(struct cpu_state_32 *state) {
    if (state->int_no < 32) {
        serial_printf("32-bit CPU EXCEPTION: %d\n", (int)state->int_no);
        for (;;);
    }
}

void irq_handler(struct cpu_state_32 *state) {
    if (interrupt_handlers[state->int_no]) {
        interrupt_handlers[state->int_no]((struct interrupt_frame*)state);
    }
    if (state->int_no >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}
