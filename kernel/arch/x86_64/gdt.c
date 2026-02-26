#include "gdt.h"
#include <stdint.h>
#include <string.h>

/* ── Internal storage ─────────────────────────────────────────────────────── */
/* 5 normal entries (null, k-code, k-data, u-code, u-data) + 2 for TSS = 7   */
#define GDT_ENTRIES 7

static gdt_entry_t  gdt[GDT_ENTRIES];
static gdt_ptr_t    gdt_ptr;
static tss_t        tss;

/* Defined in isr.asm – reloads segment registers after lgdt */
extern void gdt_flush(uint64_t gdt_ptr_addr);
extern void tss_flush(void);

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran)
{
    gdt[idx].base_low   = (base  & 0xFFFF);
    gdt[idx].base_mid   = (base  >> 16) & 0xFF;
    gdt[idx].base_high  = (base  >> 24) & 0xFF;
    gdt[idx].limit_low  = (limit & 0xFFFF);
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].access     = access;
}

/* TSS is a 16-byte system descriptor in 64-bit mode */
static void gdt_set_tss(int idx, uint64_t base, uint32_t limit)
{
    gdt_tss_entry_t *tss_entry = (gdt_tss_entry_t *)&gdt[idx];
    tss_entry->limit_low  = (limit & 0xFFFF);
    tss_entry->base_low   = (base  & 0xFFFF);
    tss_entry->base_mid   = (base  >> 16) & 0xFF;
    tss_entry->access     = 0x89;   /* Present, DPL=0, 64-bit TSS (available) */
    tss_entry->granularity = ((limit >> 16) & 0x0F);
    tss_entry->base_high  = (base  >> 24) & 0xFF;
    tss_entry->base_upper = (base  >> 32) & 0xFFFFFFFF;
    tss_entry->reserved   = 0;
}

/* ── Public: gdt_init ─────────────────────────────────────────────────────── */
void gdt_init(void)
{
    /* Clear TSS */
    memset(&tss, 0, sizeof(tss));
    tss.iopb_offset = sizeof(tss);     /* No I/O permission bitmap */

    /* stack_top is the symbol at the top of the 64KiB boot stack in .bss.
     * Declare it as a linker symbol (uintptr_t) rather than an array to avoid
     * GCC treating it as an array base address (which would be the same value
     * but emits different relocation types that upset some hypervisors). */
    extern uintptr_t stack_top;
    tss.rsp0 = (uint64_t)&stack_top;

    /* ── Descriptors ────────────────────────────────────────────────────────
     *  Granularity byte upper nibble (bit7..4):
     *    bit7 = G  (granularity: 0=byte, 1=4K)
     *    bit6 = D/B (default size: 1=32-bit for data; must be 0 for 64-bit code)
     *    bit5 = L  (long mode code: 1=64-bit; MUST be 0 for data descriptors)
     *    bit4 = AVL
     *  Code (64-bit): G=1 D=0 L=1  → upper nibble 0xA → gran=0xA0
     *  Data (flat):   G=1 D=1 L=0  → upper nibble 0xC → gran=0xC0
     *  VMware VT-x guest-state validation REJECTS data descriptors with L=1.
     * ─────────────────────────────────────────────────────────────────────── */

    /* 0: Null descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* 1: Kernel code  (0x08) – 64-bit, DPL=0, L=1, D=0 */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    /* 2: Kernel data  (0x10) – DPL=0, L=0, D=1  (gran=0xC0, NOT 0xA0) */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);

    /* 3: User code    (0x18) – 64-bit, DPL=3, L=1, D=0 */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);

    /* 4: User data    (0x20) – DPL=3, L=0, D=1  (gran=0xC0, NOT 0xA0) */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);

    /* 5-6: TSS (two consecutive entries, 16 bytes total) */
    gdt_set_tss(5, (uint64_t)&tss, sizeof(tss) - 1);

    /* ── GDT pointer ────────────────────────────────────────────────────── */
    gdt_ptr.size    = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.address = (uint64_t)gdt;

    /* Load GDT and reload segment registers */
    gdt_flush((uint64_t)&gdt_ptr);

    /* Load TSS into TR */
    tss_flush();
}

/* Update ring-0 stack pointer (call on every task/context switch) */
void gdt_set_kernel_stack(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}
