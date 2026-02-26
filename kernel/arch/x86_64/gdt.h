#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* GDT segment selector indices (byte offsets into GDT) */
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_CODE    0x18
#define GDT_USER_DATA    0x20
#define GDT_TSS          0x28

/* ── GDT Entry (8 bytes) ───────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;     /* Bits  0–15 of segment limit          */
    uint16_t base_low;      /* Bits  0–15 of base address           */
    uint8_t  base_mid;      /* Bits 16–23 of base address           */
    uint8_t  access;        /* Access flags (type, DPL, present)    */
    uint8_t  granularity;   /* Granularity + upper limit bits       */
    uint8_t  base_high;     /* Bits 24–31 of base address           */
} gdt_entry_t;

/* ── TSS Descriptor (16 bytes, doubled entry) ─────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} gdt_tss_entry_t;

/* ── GDT Pointer (for lgdt) ───────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t size;          /* Table size - 1                       */
    uint64_t address;       /* Linear address of the GDT            */
} gdt_ptr_t;

/* ── 64-bit Task State Segment ─────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;          /* Ring-0 stack pointer                 */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];        /* Interrupt Stack Table entries        */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;   /* I/O permission bitmap offset         */
} tss_t;

/* ── Public Interface ─────────────────────────────────────────────────────── */
void gdt_init(void);
void gdt_set_kernel_stack(uint64_t rsp0);

#endif /* GDT_H */
