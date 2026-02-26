/* =============================================================================
 * SporkOS Linker Script
 * Produces a flat binary image loaded by GRUB at 1 MiB (0x100000)
 * ============================================================================= */

ENTRY(_start)

SECTIONS
{
    /* Kernel is loaded at 1 MiB physical */
    . = 1M;

    /* Multiboot header MUST appear within the first 32 KiB of the file */
    .multiboot_header :
    {
        KEEP(*(.multiboot_header))
    }

    /* Read-only text (code) */
    .text ALIGN(4K) :
    {
        *(.text .text.*)
    }

    /* Read-only data */
    .rodata ALIGN(4K) :
    {
        *(.rodata .rodata.*)
    }

    /* Initialized read-write data */
    .data ALIGN(4K) :
    {
        *(.data .data.*)
    }

    /* Zero-initialized data */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    /* Record kernel end for memory manager */
    kernel_end = .;
}
