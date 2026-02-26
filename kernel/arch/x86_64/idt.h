#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* ── Gate types ───────────────────────────────────────────────────────────── */
#define IDT_GATE_INTERRUPT  0x8E   /* Present, DPL=0, 64-bit interrupt gate  */
#define IDT_GATE_TRAP       0x8F   /* Present, DPL=0, 64-bit trap gate       */
#define IDT_GATE_USER_INT   0xEE   /* Present, DPL=3, 64-bit interrupt gate  */

/* ── IDT Entry (16 bytes) ─────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t isr_low;       /* ISR address bits  0–15                */
    uint16_t kernel_cs;     /* Kernel code segment selector          */
    uint8_t  ist;           /* Interrupt Stack Table index (0 = none)*/
    uint8_t  attributes;    /* Gate type, DPL, present               */
    uint16_t isr_mid;       /* ISR address bits 16–31                */
    uint32_t isr_high;      /* ISR address bits 32–63                */
    uint32_t reserved;      /* Always 0                              */
} idt_entry_t;

/* ── IDT Pointer (for lidt) ───────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t size;
    uint64_t address;
} idt_ptr_t;

/* ── CPU register snapshot pushed by ISR stubs ───────────────────────────── */
typedef struct __attribute__((packed)) {
    /* Pushed by isr.asm push_regs macro: */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    /* Pushed by the stub (interrupt number + optional error code): */
    uint64_t int_no, err_code;
    /* Pushed automatically by the CPU on interrupt: */
    uint64_t rip, cs, rflags, rsp, ss;
} interrupt_frame_t;

/* ── Public Interface ─────────────────────────────────────────────────────── */
void idt_init(void);
void idt_set_gate(uint8_t vector, void *isr, uint8_t flags);
void irq_register_handler(uint8_t irq, void (*handler)(interrupt_frame_t *));

/* C-level interrupt dispatcher – called from isr.asm */
void interrupt_handler(interrupt_frame_t *frame);

#endif /* IDT_H */
