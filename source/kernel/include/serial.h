#ifndef JURKOS_SERIAL_H
#define JURKOS_SERIAL_H

#include "types.h"

#define COM1 0x3F8

void serial_init(void);
void serial_putc(char c);
void serial_print(const char *str);
void serial_println(const char *str);
void serial_printf(const char *fmt, ...);

#endif
