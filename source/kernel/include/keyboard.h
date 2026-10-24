#ifndef JURKOS_KEYBOARD_H
#define JURKOS_KEYBOARD_H

#include "types.h"

void keyboard_init(void);
char keyboard_getchar(void);
bool keyboard_has_char(void);
void keyboard_readline(char *buffer, size_t max_len);

#endif
