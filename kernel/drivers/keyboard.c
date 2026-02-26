#include "keyboard.h"
#include "vga.h"
#include <stdint.h>
#include <stdbool.h>

/* ── US QWERTY scancode-to-ASCII tables ──────────────────────────────────── */
static const char scancode_normal[128] = {
/*00*/  0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
/*0E*/  '\b', '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
/*1C*/  '\n', 0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';','\'', '`',
/*2A*/  0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*',
/*38*/  0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
/*47*/  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',
/*54..7F*/ [0x54 ... 0x7F] = 0
};

static const char scancode_shift[128] = {
/*00*/  0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
/*0E*/  '\b', '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
/*1C*/  '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
/*2A*/  0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
/*38*/  0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
/*47*/  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',
/*54..7F*/ [0x54 ... 0x7F] = 0
};

/* ── Internal ring buffer ─────────────────────────────────────────────────── */
static volatile char kb_buf[KB_BUFFER_SIZE];
static volatile uint16_t kb_head = 0;
static volatile uint16_t kb_tail = 0;
static volatile uint8_t  key_flags = 0;

static inline uint8_t inb(uint16_t port)
{
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* ── IRQ1 handler ─────────────────────────────────────────────────────────── */
static void keyboard_irq_handler(interrupt_frame_t *frame)
{
    (void)frame;

    uint8_t scancode = inb(0x60);
    bool    released = (scancode & 0x80) != 0;
    uint8_t key      = scancode & 0x7F;

    /* Handle modifier keys */
    if (key == 0x2A || key == 0x36) {           /* Left/Right Shift */
        if (released) key_flags &= ~KEY_SHIFT;
        else          key_flags |=  KEY_SHIFT;
        return;
    }
    if (key == 0x1D) {                           /* Left CTRL */
        if (released) key_flags &= ~KEY_CTRL;
        else          key_flags |=  KEY_CTRL;
        return;
    }
    if (key == 0x38) {                           /* Left ALT */
        if (released) key_flags &= ~KEY_ALT;
        else          key_flags |=  KEY_ALT;
        return;
    }
    if (key == 0x3A && !released) {              /* Caps Lock toggle */
        key_flags ^= KEY_CAPS;
        return;
    }

    if (released) return;   /* Ignore all other key-up events */

    /* Translate scancode to ASCII */
    char c = 0;
    if (key < 128) {
        bool shift = (key_flags & KEY_SHIFT) != 0;
        /* Caps Lock inverts shift for alphabetic keys */
        if ((key_flags & KEY_CAPS) &&
            ((scancode_normal[key] >= 'a' && scancode_normal[key] <= 'z') ||
             (scancode_normal[key] >= 'A' && scancode_normal[key] <= 'Z')))
            shift = !shift;

        c = shift ? scancode_shift[key] : scancode_normal[key];
    }

    if (!c) return;

    /* Buffer overflow guard: drop character if full */
    uint16_t next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_tail) {
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

/* ── keyboard_init ─────────────────────────────────────────────────────────── */
void keyboard_init(void)
{
    key_flags = 0;
    kb_head = kb_tail = 0;

    /* Wait for PS/2 controller to be ready, then flush output buffer */
    while (inb(0x64) & 0x01)
        inb(0x60);

    /* Register the IRQ1 handler */
    extern void irq_register_handler(uint8_t irq,
                                      void (*handler)(interrupt_frame_t *));
    irq_register_handler(1, keyboard_irq_handler);
}

/* ── keyboard_getchar: blocks until a character is available ─────────────── */
char keyboard_getchar(void)
{
    while (kb_head == kb_tail)
        __asm__ volatile("hlt");

    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

/* ── keyboard_trygetchar: non-blocking poll ───────────────────────────────── */
int keyboard_trygetchar(char *out)
{
    if (kb_head == kb_tail) return 0;
    *out = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return 1;
}

bool keyboard_is_shift(void) { return (key_flags & KEY_SHIFT) != 0; }
bool keyboard_is_ctrl(void)  { return (key_flags & KEY_CTRL)  != 0; }
