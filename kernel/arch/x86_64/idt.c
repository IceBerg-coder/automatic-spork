#include "idt.h"
#include "gdt.h"
#include "../../drivers/vga.h"
#include "../../drivers/keyboard.h"
#include <stdint.h>
#include <string.h>

/* Exception names for display */
static const char *exception_names[] = {
    "#DE Division Error",
    "#DB Debug",
    "NMI Non-Maskable Interrupt",
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack Segment Fault",
    "#GP General Protection Fault",
    "#PF Page Fault",
    "Reserved",
    "#MF x87 Floating-Point Exception",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XF SIMD Floating-Point Exception",
    "#VE Virtualization Exception",
    "#CP Control Protection Exception",
};
#define EXCEPTION_COUNT 22

/* ISR entry stubs declared in isr.asm */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

/* IRQ stubs (hardware interrupts 32–47) */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

/* IRQ handler function pointer table */
static void (*irq_handlers[16])(interrupt_frame_t *) = {0};

/* ── idt_set_gate ─────────────────────────────────────────────────────────── */
void idt_set_gate(uint8_t vector, void *isr, uint8_t flags)
{
    uint64_t addr = (uint64_t)isr;
    idt[vector].isr_low    = (uint16_t)(addr & 0xFFFF);
    idt[vector].isr_mid    = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].isr_high   = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vector].kernel_cs  = GDT_KERNEL_CODE;
    idt[vector].ist        = 0;
    idt[vector].attributes = flags;
    idt[vector].reserved   = 0;
}

/* ── Reprogram PICs to map IRQs 0–15 → vectors 32–47 ─────────────────────── */
static void pic_remap(void)
{
    /* ICW1 */
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x11), "Nd"((uint16_t)0x20));
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x11), "Nd"((uint16_t)0xA0));
    /* ICW2: vector offsets */
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x20), "Nd"((uint16_t)0x21));
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x28), "Nd"((uint16_t)0xA1));
    /* ICW3 */
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x04), "Nd"((uint16_t)0x21));
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x02), "Nd"((uint16_t)0xA1));
    /* ICW4: 8086 mode */
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x01), "Nd"((uint16_t)0x21));
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x01), "Nd"((uint16_t)0xA1));
    /* Mask all IRQs initially – enable selectively */
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0xFD), "Nd"((uint16_t)0x21)); /* Enable IRQ1 (keyboard) */
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0xFF), "Nd"((uint16_t)0xA1));
}

/* ── idt_init ─────────────────────────────────────────────────────────────── */
void idt_init(void)
{
    memset(idt, 0, sizeof(idt));

    /* Exceptions 0–31 */
    idt_set_gate(0,  isr0,  IDT_GATE_INTERRUPT);
    idt_set_gate(1,  isr1,  IDT_GATE_INTERRUPT);
    idt_set_gate(2,  isr2,  IDT_GATE_INTERRUPT);
    idt_set_gate(3,  isr3,  IDT_GATE_TRAP);
    idt_set_gate(4,  isr4,  IDT_GATE_INTERRUPT);
    idt_set_gate(5,  isr5,  IDT_GATE_INTERRUPT);
    idt_set_gate(6,  isr6,  IDT_GATE_INTERRUPT);
    idt_set_gate(7,  isr7,  IDT_GATE_INTERRUPT);
    idt_set_gate(8,  isr8,  IDT_GATE_INTERRUPT);
    idt_set_gate(9,  isr9,  IDT_GATE_INTERRUPT);
    idt_set_gate(10, isr10, IDT_GATE_INTERRUPT);
    idt_set_gate(11, isr11, IDT_GATE_INTERRUPT);
    idt_set_gate(12, isr12, IDT_GATE_INTERRUPT);
    idt_set_gate(13, isr13, IDT_GATE_INTERRUPT);
    idt_set_gate(14, isr14, IDT_GATE_INTERRUPT);
    idt_set_gate(15, isr15, IDT_GATE_INTERRUPT);
    idt_set_gate(16, isr16, IDT_GATE_INTERRUPT);
    idt_set_gate(17, isr17, IDT_GATE_INTERRUPT);
    idt_set_gate(18, isr18, IDT_GATE_INTERRUPT);
    idt_set_gate(19, isr19, IDT_GATE_INTERRUPT);
    idt_set_gate(20, isr20, IDT_GATE_INTERRUPT);
    idt_set_gate(21, isr21, IDT_GATE_INTERRUPT);
    idt_set_gate(22, isr22, IDT_GATE_INTERRUPT);
    idt_set_gate(23, isr23, IDT_GATE_INTERRUPT);
    idt_set_gate(24, isr24, IDT_GATE_INTERRUPT);
    idt_set_gate(25, isr25, IDT_GATE_INTERRUPT);
    idt_set_gate(26, isr26, IDT_GATE_INTERRUPT);
    idt_set_gate(27, isr27, IDT_GATE_INTERRUPT);
    idt_set_gate(28, isr28, IDT_GATE_INTERRUPT);
    idt_set_gate(29, isr29, IDT_GATE_INTERRUPT);
    idt_set_gate(30, isr30, IDT_GATE_INTERRUPT);
    idt_set_gate(31, isr31, IDT_GATE_INTERRUPT);

    /* Hardware IRQ stubs (vectors 32–47) */
    idt_set_gate(32, irq0,  IDT_GATE_INTERRUPT);
    idt_set_gate(33, irq1,  IDT_GATE_INTERRUPT);
    idt_set_gate(34, irq2,  IDT_GATE_INTERRUPT);
    idt_set_gate(35, irq3,  IDT_GATE_INTERRUPT);
    idt_set_gate(36, irq4,  IDT_GATE_INTERRUPT);
    idt_set_gate(37, irq5,  IDT_GATE_INTERRUPT);
    idt_set_gate(38, irq6,  IDT_GATE_INTERRUPT);
    idt_set_gate(39, irq7,  IDT_GATE_INTERRUPT);
    idt_set_gate(40, irq8,  IDT_GATE_INTERRUPT);
    idt_set_gate(41, irq9,  IDT_GATE_INTERRUPT);
    idt_set_gate(42, irq10, IDT_GATE_INTERRUPT);
    idt_set_gate(43, irq11, IDT_GATE_INTERRUPT);
    idt_set_gate(44, irq12, IDT_GATE_INTERRUPT);
    idt_set_gate(45, irq13, IDT_GATE_INTERRUPT);
    idt_set_gate(46, irq14, IDT_GATE_INTERRUPT);
    idt_set_gate(47, irq15, IDT_GATE_INTERRUPT);

    /* Remap PIC */
    pic_remap();

    /* Load IDT */
    idt_ptr.size    = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.address = (uint64_t)idt;
    __asm__ volatile("lidt %0" :: "m"(idt_ptr));

    /* Enable interrupts */
    __asm__ volatile("sti");
}

/* ── Register an IRQ handler ─────────────────────────────────────────────── */
void irq_register_handler(uint8_t irq, void (*handler)(interrupt_frame_t *))
{
    if (irq < 16)
        irq_handlers[irq] = handler;
}

/* ── Central interrupt dispatcher (called from isr.asm) ──────────────────── */
void interrupt_handler(interrupt_frame_t *frame)
{
    uint64_t vec = frame->int_no;

    if (vec < 32) {
        /* ── CPU Exception ───────────────────────────────────────────────── */
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
        vga_puts("\n\n *** KERNEL EXCEPTION ***\n ");
        if (vec < EXCEPTION_COUNT)
            vga_puts(exception_names[vec]);
        vga_puts("\n RIP=0x");
        vga_print_hex(frame->rip);
        vga_puts("  ERR=0x");
        vga_print_hex(frame->err_code);
        vga_puts("\n RSP=0x");
        vga_print_hex(frame->rsp);
        vga_puts("  CS=0x");
        vga_print_hex(frame->cs);
        vga_puts("\n\n System halted.\n");
        for (;;) __asm__ volatile("hlt");

    } else if (vec < 48) {
        /* ── Hardware IRQ ────────────────────────────────────────────────── */
        uint8_t irq = (uint8_t)(vec - 32);

        /* Dispatch to registered handler */
        if (irq_handlers[irq])
            irq_handlers[irq](frame);

        /* Send End-Of-Interrupt to PIC(s) */
        if (irq >= 8)
            __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x20), "Nd"((uint16_t)0xA0));
        __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
    }
}
