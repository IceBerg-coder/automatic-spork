; =============================================================================
; SporkOS - ISR / IRQ Stubs + GDT/TSS flush helpers
; All stubs save registers, call the C dispatcher, then restore and iretq
; =============================================================================

global gdt_flush
global tss_flush
extern interrupt_handler

; ── Macros ──────────────────────────────────────────────────────────────────

; Save all general-purpose registers onto the stack
%macro push_regs 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

; Restore all general-purpose registers from the stack
%macro pop_regs 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; ISR stub for exceptions WITHOUT an error code (CPU does not push one)
%macro isr_no_err 1
global isr%1
isr%1:
    push qword 0            ; Fake error code
    push qword %1           ; Interrupt number
    jmp isr_common
%endmacro

; ISR stub for exceptions WITH an error code (CPU already pushed it)
%macro isr_err 1
global isr%1
isr%1:
    push qword %1           ; Interrupt number (error code already on stack)
    jmp isr_common
%endmacro

; IRQ stub (maps IRQ n to vector 32+n)
%macro irq_stub 1
global irq%1
irq%1:
    push qword 0            ; No error code
    push qword (32 + %1)    ; Vector number
    jmp isr_common
%endmacro

; =============================================================================
; Exception Stubs (vectors 0–31)
; Vectors with CPU-pushed error codes: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
; =============================================================================
section .text
bits 64

isr_no_err  0   ; #DE Division Error
isr_no_err  1   ; #DB Debug
isr_no_err  2   ; NMI
isr_no_err  3   ; #BP Breakpoint
isr_no_err  4   ; #OF Overflow
isr_no_err  5   ; #BR Bound Range
isr_no_err  6   ; #UD Invalid Opcode
isr_no_err  7   ; #NM Device Not Available
isr_err     8   ; #DF Double Fault
isr_no_err  9   ; Coprocessor Segment Overrun (legacy)
isr_err     10  ; #TS Invalid TSS
isr_err     11  ; #NP Segment Not Present
isr_err     12  ; #SS Stack Segment Fault
isr_err     13  ; #GP General Protection Fault
isr_err     14  ; #PF Page Fault
isr_no_err  15  ; Reserved
isr_no_err  16  ; #MF x87 FP Exception
isr_err     17  ; #AC Alignment Check
isr_no_err  18  ; #MC Machine Check
isr_no_err  19  ; #XF SIMD FP Exception
isr_no_err  20  ; #VE Virtualization Exception
isr_err     21  ; #CP Control Protection
isr_no_err  22
isr_no_err  23
isr_no_err  24
isr_no_err  25
isr_no_err  26
isr_no_err  27
isr_no_err  28
isr_err     29
isr_err     30
isr_no_err  31

; =============================================================================
; IRQ Stubs (hardware interrupts, vectors 32–47)
; =============================================================================
irq_stub 0   ; IRQ0  – PIT Timer
irq_stub 1   ; IRQ1  – PS/2 Keyboard
irq_stub 2   ; IRQ2  – PIC cascade
irq_stub 3   ; IRQ3  – COM2
irq_stub 4   ; IRQ4  – COM1
irq_stub 5   ; IRQ5  – LPT2 / Sound
irq_stub 6   ; IRQ6  – Floppy
irq_stub 7   ; IRQ7  – LPT1 (spurious)
irq_stub 8   ; IRQ8  – CMOS Real-Time Clock
irq_stub 9   ; IRQ9
irq_stub 10  ; IRQ10
irq_stub 11  ; IRQ11
irq_stub 12  ; IRQ12 – PS/2 Mouse
irq_stub 13  ; IRQ13 – FPU / coprocessor
irq_stub 14  ; IRQ14 – Primary ATA
irq_stub 15  ; IRQ15 – Secondary ATA

; =============================================================================
; Common ISR handler: save context → call C → restore context → iretq
; At entry the stack looks like:
;   [rsp+00] int_no
;   [rsp+08] err_code     ← CPU or fake
;   [rsp+16] rip          ← pushed by CPU
;   [rsp+24] cs
;   [rsp+32] rflags
;   [rsp+40] rsp (old)
;   [rsp+48] ss
; After push_regs: rsp points to interrupt_frame_t.r15
; =============================================================================
isr_common:
    push_regs

    ; Align stack to 16-byte boundary as required by System V ABI
    mov rbp, rsp
    and rsp, ~0xF

    ; Pass pointer to interrupt_frame_t as first argument (RDI)
    mov rdi, rbp
    call interrupt_handler

    ; Restore stack alignment
    mov rsp, rbp

    pop_regs

    ; Remove int_no and err_code that were pushed by stub
    add rsp, 16

    iretq

; Mark stack as non-executable (suppresses GNU-stack linker warning)
section .note.GNU-stack noalloc noexec nowrite progbits

; =============================================================================
; GDT flush: reload CS via a far return trick, then update data segments
; Argument: RDI = pointer to gdt_ptr_t
; =============================================================================
gdt_flush:
    lgdt [rdi]

    ; Reload code segment via a far return
    push qword 0x08         ; New CS = kernel code selector
    lea  rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    ; Reload data segments
    mov ax, 0x10            ; Kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

; =============================================================================
; TSS flush: load the Task Register with the TSS selector (index 5 × 8 = 0x28)
; =============================================================================
tss_flush:
    mov ax, 0x28
    ltr ax
    ret
