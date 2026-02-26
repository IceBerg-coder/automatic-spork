#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include "../arch/x86_64/idt.h"

/* ── Key state flags ──────────────────────────────────────────────────────── */
#define KEY_SHIFT   0x01
#define KEY_CTRL    0x02
#define KEY_ALT     0x04
#define KEY_CAPS    0x08

/* Input buffer capacity */
#define KB_BUFFER_SIZE  256

/* ── Public API ───────────────────────────────────────────────────────────── */
void keyboard_init(void);                /* Install IRQ1 handler             */
char keyboard_getchar(void);             /* Block until a key is available   */
int  keyboard_trygetchar(char *out);     /* Non-blocking; returns 1 on char  */
bool keyboard_is_shift(void);
bool keyboard_is_ctrl(void);

#endif /* KEYBOARD_H */
