#include "../include/serial.h"
#include "../include/io.h"

void serial_init(void) {
    outb(COM1 + 1, 0x00); // Disable all interrupts
    outb(COM1 + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00); //                  (hi byte)
    outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

static int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    while (serial_is_transmit_empty() == 0);
    outb(COM1, c);
}

void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putc('\r');
        }
        serial_putc(*str++);
    }
}

void serial_println(const char *str) {
    serial_print(str);
    serial_putc('\r');
    serial_putc('\n');
}

static void serial_print_hex(uint64_t val) {
    const char hex_chars[] = "0123456789abcdef";
    char buf[17];
    buf[16] = '\0';
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    serial_print(buf);
}

static void serial_print_dec(int64_t val) {
    if (val < 0) {
        serial_putc('-');
        val = -val;
    }
    if (val == 0) {
        serial_putc('0');
        return;
    }
    char buf[32];
    int idx = 0;
    while (val > 0) {
        buf[idx++] = '0' + (val % 10);
        val /= 10;
    }
    while (idx > 0) {
        serial_putc(buf[--idx]);
    }
}

void serial_printf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++;
            if (fmt[i] == 's') {
                const char *s = __builtin_va_arg(args, const char*);
                serial_print(s ? s : "(null)");
            } else if (fmt[i] == 'd') {
                int d = __builtin_va_arg(args, int);
                serial_print_dec(d);
            } else if (fmt[i] == 'x') {
                uint64_t x = __builtin_va_arg(args, uint64_t);
                serial_print_hex(x);
            } else if (fmt[i] == 'c') {
                char c = (char)__builtin_va_arg(args, int);
                serial_putc(c);
            } else if (fmt[i] == '%') {
                serial_putc('%');
            }
        } else {
            if (fmt[i] == '\n') serial_putc('\r');
            serial_putc(fmt[i]);
        }
    }

    __builtin_va_end(args);
}
