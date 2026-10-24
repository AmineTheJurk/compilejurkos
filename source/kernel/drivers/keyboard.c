#include "../include/keyboard.h"
#include "../include/io.h"
#include "../include/idt.h"
#include "../include/framebuffer.h"

#define KBD_BUFFER_SIZE 256
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;
static bool shift_pressed = false;

static const char kbd_scancodes[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, /* Right shift */
    '*',
    0,  /* Alt */
    ' ', /* Space */
    0,  /* Caps lock */
};

static const char kbd_scancodes_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, /* Ctrl */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, /* Left shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0, /* Right shift */
    '*',
    0,  /* Alt */
    ' ', /* Space */
    0,  /* Caps lock */
};

static void keyboard_interrupt_handler(struct interrupt_frame *frame) {
    (void)frame;
    uint8_t scancode = inb(0x60);

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return;
    }

    // Ignore key release
    if (scancode & 0x80) return;

    if (scancode < 128) {
        char c = shift_pressed ? kbd_scancodes_shift[scancode] : kbd_scancodes[scancode];
        if (c) {
            int next = (kbd_head + 1) % KBD_BUFFER_SIZE;
            if (next != kbd_tail) {
                kbd_buffer[kbd_head] = c;
                kbd_head = next;
            }
        }
    }
}

void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_interrupt_handler);
}

bool keyboard_has_char(void) {
    return kbd_head != kbd_tail;
}

char keyboard_getchar(void) {
    while (!keyboard_has_char()) {
        __asm__ volatile ("hlt");
    }
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

void keyboard_readline(char *buffer, size_t max_len) {
    size_t idx = 0;
    while (idx < max_len - 1) {
        char c = keyboard_getchar();
        if (c == '\n' || c == '\r') {
            fb_print("\n");
            break;
        } else if (c == '\b') {
            if (idx > 0) {
                idx--;
                fb_print("\b \b");
            }
        } else if (c >= 32 && c <= 126) {
            buffer[idx++] = c;
            char str[2] = {c, '\0'};
            fb_print(str);
        }
    }
    buffer[idx] = '\0';
}
