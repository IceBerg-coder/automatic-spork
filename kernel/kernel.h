#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Kernel entry point ───────────────────────────────────────────────────── */
void kernel_main(uint32_t mb2_magic, void *mb2_info)
    __attribute__((noreturn));

/* ── Panic ────────────────────────────────────────────────────────────────── */
static inline void __attribute__((noreturn)) kernel_panic(const char *msg)
{
    (void)msg;
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

/* ── Port I/O inlines ─────────────────────────────────────────────────────── */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void)
{
    outb(0x80, 0x00);
}

/* ── IRQ registration (declared in idt.c, prototype also in idt.h) ─────────── */
#include "arch/x86_64/idt.h"

#endif /* KERNEL_H */
