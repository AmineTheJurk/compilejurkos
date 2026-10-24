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
    idt_entries[num].ist         = 0;
    idt_entries[num].type_attr   = flags;
    idt_entries[num].offset_mid  = (base >> 16) & 0xFFFF;
    idt_entries[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt_entries[num].zero        = 0;
}

static void pic_remap(void) {
    // ICW1
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    // ICW2: master offset 32, slave offset 40
    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();

    // ICW3: cascade wiring
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();

    // ICW4: 8086 mode
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    // Enable standard IRQs (timer, keyboard, cascade)
    outb(0x21, 0xFC); // 11111100b (enable IRQ0 timer & IRQ1 keyboard)
    outb(0xA1, 0xFF);
}

void idt_init(void) {
    idt_pointer.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_pointer.base  = (uintptr_t)&idt_entries;

    memset(&idt_entries, 0, sizeof(struct idt_entry) * 256);

    pic_remap();

    // 0x8E = Present, Ring 0, 64-bit Interrupt Gate
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

    // IRQs 32..47
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

struct cpu_state {
    uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8;
    uint64_t rax, rcx, rdx, rsi, rdi;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

#include "../../include/speaker.h"
#include "../../include/framebuffer.h"

static const char *exception_messages[32] = {
    "Division by Zero (#DE)",
    "Debug (#DB)",
    "Non-Maskable Interrupt (#NMI)",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved",
    "x87 Floating-Point Exception (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point Exception (#XM)",
    "Virtualization Exception (#VE)",
    "Control Protection Exception (#CP)",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception (#SX)",
    "Reserved"
};

void isr_handler(struct cpu_state *state) {
    if (state->int_no < 32) {
        const char *reason = exception_messages[state->int_no];
        uint64_t cr2 = 0;
        if (state->int_no == 14) {
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        }

        serial_printf("\n=======================================================\n");
        serial_printf(" !!! KERNEL PANIC: %s !!!\n", reason);
        serial_printf(" Code Location (RIP): 0x%x  Error Code: 0x%x\n", (uint64_t)state->rip, (uint32_t)state->err_code);
        if (state->int_no == 14) {
            serial_printf(" Faulting Memory Address (CR2): 0x%x\n", cr2);
        }
        serial_printf(" Stack Pointer (RSP): 0x%x\n", (uint64_t)state->rsp);
        serial_printf(" RAX=0x%x RBX=0x%x RCX=0x%x RDX=0x%x\n", state->rax, state->rbx, state->rcx, state->rdx);
        serial_printf(" System halted.\n");
        serial_printf("=======================================================\n");

        fb_clear(COLOR_RED);
        fb_draw_box(80, 100, 640, 240, COLOR_BLUE, COLOR_WHITE,
                    " !!! JURKOS KERNEL PANIC !!! ",
                    "\n A fatal CPU hardware exception has occurred in JurkOS!\n\n"
                    " Reason: A CPU fault was encountered during execution.\n"
                    " What Went Wrong: The instruction attempted an invalid memory or CPU operation.\n"
                    " Diagnostic details have been logged to the system COM1 console.\n\n"
                    " System halted to prevent hardware corruption. Please reboot.");

        fb_print_color("\n  [PANIC] ", COLOR_WHITE, COLOR_RED);
        fb_print_color(reason, COLOR_YELLOW, COLOR_RED);
        fb_print_color("\n", COLOR_WHITE, COLOR_RED);

        // Sound hardware PC speaker beep siren
        speaker_panic_siren();

        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
}

void irq_handler(struct cpu_state *state) {
    if (interrupt_handlers[state->int_no]) {
        interrupt_handlers[state->int_no]((struct interrupt_frame*)state);
    }

    // Send EOI to PIC
    if (state->int_no >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}
