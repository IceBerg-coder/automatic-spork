; =============================================================================
; SporkOS - 64-bit Operating System Bootloader (Multiboot2 + Long Mode)
; Transitions from 32-bit protected mode (GRUB) to 64-bit long mode
; =============================================================================

; Mark stack as non-executable (suppresses GNU-stack linker warning)
section .note.GNU-stack noalloc noexec nowrite progbits

global _start
global gdt64_pointer_low
global stack_top

extern kernel_main

; =============================================================================
; Multiboot2 Header
; =============================================================================
section .multiboot_header
align 8
mb2_header_start:
    dd 0xe85250d6                                              ; Magic number
    dd 0                                                       ; Architecture: i386
    dd mb2_header_end - mb2_header_start                       ; Header length
    dd 0x100000000 - (0xe85250d6 + 0 + (mb2_header_end - mb2_header_start)) ; Checksum
    ; Framebuffer tag
    dw 5                    ; type: framebuffer
    dw 1                    ; flags: optional
    dd 20                   ; size
    dd 0                    ; width (0 = no preference)
    dd 0                    ; height
    dd 0                    ; depth
    align 8
    ; End tag
    dw 0
    dw 0
    dd 8
mb2_header_end:

; =============================================================================
; 32-bit Entry Point (GRUB drops us here in 32-bit protected mode)
; =============================================================================
section .text
bits 32

_start:
    ; Disable interrupts, clear direction flag
    cli
    cld

    ; VGA checkpoint A: white 'A' on blue at column 0  (visible if we reach _start)
    mov word [0xb8000], 0x1f41

    ; Set up initial stack
    mov esp, stack_top

    ; Save multiboot2 magic and info pointer
    mov [mb2_magic], eax
    mov [mb2_info],  ebx

    ; VGA checkpoint B
    mov word [0xb8002], 0x1f42

    ; Verify CPU supports long mode
    call check_cpuid
    call check_longmode

    ; VGA checkpoint C: past CPUID/LM checks
    mov word [0xb8004], 0x1f43

    ; Set up initial page tables
    call setup_page_tables

    ; VGA checkpoint D: page tables built
    mov word [0xb8006], 0x1f44

    ; Enable PAE (Physical Address Extension)
    mov eax, cr4
    or  eax, (1 << 5)       ; PAE bit
    mov cr4, eax

    ; Load PML4 table address into CR3
    mov eax, pml4_table
    mov cr3, eax

    ; Enable long mode via EFER MSR
    mov ecx, 0xC0000080     ; IA32_EFER MSR
    rdmsr
    or  eax, (1 << 8)       ; Long Mode Enable (LME)
    wrmsr

    ; VGA checkpoint E: EFER.LME set
    mov word [0xb8008], 0x1f45

    ; Enable paging + protected mode
    mov eax, cr0
    or  eax, (1 << 31)      ; PG: enable paging
    or  eax, (1 << 0)       ; PE: protected mode
    mov cr0, eax

    ; VGA checkpoint F: paging enabled (we are now in compatibility mode)
    mov word [0xb800a], 0x1f46

    ; Load the 64-bit GDT
    lgdt [gdt64_pointer_low]

    ; Far jump to 64-bit code segment → flushes pipeline, enters long mode
    jmp 0x08:long_mode_start

; =============================================================================
; CPUID check: verify CPUID instruction is available
; =============================================================================
check_cpuid:
    ; Flip bit 21 (ID) in EFLAGS. If we can flip it, CPUID is supported.
    pushfd
    pop  eax
    mov  ecx, eax
    xor  eax, (1 << 21)
    push eax
    popfd
    pushfd
    pop  eax
    push ecx
    popfd
    cmp  eax, ecx
    je   .no_cpuid
    ret
.no_cpuid:
    mov  al, '1'
    jmp  error_halt

; =============================================================================
; Long Mode check: verify CPU supports 64-bit long mode
; =============================================================================
check_longmode:
    ; Check extended CPUID functions exist
    mov  eax, 0x80000000
    cpuid
    cmp  eax, 0x80000001
    jb   .no_longmode
    ; Check long mode bit
    mov  eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz   .no_longmode
    ret
.no_longmode:
    mov  al, '2'
    jmp  error_halt

; =============================================================================
; Page Table Setup
; Identity-maps the first 1 GiB with 2 MiB huge pages.
; PML4[0] → PDPT → PD[0..511] → 512 × 2 MiB pages
; Also sets PML4[256] (the canonical higher-half mirror) for future use.
; =============================================================================
setup_page_tables:
    ; Clear all three tables (4 KiB each) to avoid stale data on warm reboot
    ; cld was set at _start; repeat here for safety in case DF was modified
    cld
    push edi
    mov  edi, pml4_table
    xor  eax, eax
    mov  ecx, 3 * 4096 / 4   ; 3 pages × 4 bytes per stosd = 12288 bytes
    rep  stosd
    pop  edi

    ; PML4[0] → PDPT (present + writable)
    mov eax, pdpt_table
    or  eax, 0b11
    mov [pml4_table], eax

    ; PDPT[0] → PD (present + writable)
    mov eax, pd_table
    or  eax, 0b11
    mov [pdpt_table], eax

    ; Map 512 × 2 MiB = 1 GiB using 2 MiB huge pages
    ; imul avoids the edx clobber that a plain 'mul ecx' would produce
    mov ecx, 0
.map_pd_table:
    imul eax, ecx, 0x200000 ; eax = index * 2 MiB
    or   eax, 0b10000011    ; present + writable + huge page (PS bit)
    mov  [pd_table + ecx * 8], eax
    inc  ecx
    cmp  ecx, 512
    jne  .map_pd_table
    ret

; =============================================================================
; Error handler: prints 'E' + error code to VGA and halts
; =============================================================================
error_halt:
    ; Write "ERR:" to VGA (top-left, white on red)
    mov dword [0xb8000], 0x4f524f45   ; ER
    mov dword [0xb8004], 0x4f3a4f52   ; R:
    mov byte  [0xb8008], al
    mov byte  [0xb8009], 0x4f
.halt:
    hlt
    jmp .halt

; =============================================================================
; 64-bit Long Mode Entry
; =============================================================================
bits 64

long_mode_start:
    ; VGA checkpoint G: we are in 64-bit long mode
    mov word [0xb800c], 0x1f47

    ; Load data segments with the kernel DATA selector (0x10).
    ; IMPORTANT: SS must NOT be null – #GP → triple fault. Use data selector.
    mov ax, 0x10
    mov ss, ax
    mov ds, ax
    mov es, ax
    ; FS/GS: in 64-bit mode these are fine to set to data selector
    mov fs, ax
    mov gs, ax

    ; VGA checkpoint H: segments reloaded
    mov word [0xb800e], 0x1f48

    ; Set up a proper 64-bit stack (16-byte aligned, grows down from stack_top)
    mov rsp, stack_top
    and rsp, ~0xF

    ; Pass multiboot2 magic + info pointer to kernel_main
    ; Use 32-bit moves (zero-extends to 64-bit) since mbi is a physical address
    xor  rdi, rdi
    xor  rsi, rsi
    mov  edi, dword [mb2_magic]
    mov  esi, dword [mb2_info]

    ; VGA checkpoint I: about to call kernel_main
    mov word [0xb8010], 0x1f49

    ; Jump to C kernel
    call kernel_main

    ; Should never return – halt forever
.halt:
    hlt
    jmp .halt

; =============================================================================
; 64-bit Global Descriptor Table
; =============================================================================
section .rodata
align 8

gdt64:
    ; Null descriptor
    dq 0x0000000000000000
    ; Kernel code segment (0x08): 64-bit, P=1, DPL=0, L=1, D=0, G=1
    dq 0x00af9a000000ffff
    ; Kernel data segment (0x10): P=1, DPL=0, L=0, D=1, G=1
    ; NOTE: L MUST be 0 for data descriptors – VT-x (VMware) enforces this strictly.
    ; Using 0x00cf (G=1, D=1, L=0) not 0x00af (G=1, L=1) avoids VM-entry failure.
    dq 0x00cf92000000ffff

gdt64_pointer_low:
    dw $ - gdt64 - 1        ; Limit
    dd gdt64                ; Base (32-bit fits in 32-bit protected mode lgdt)

gdt64_pointer:
    dw $ - gdt64 - 1
    dq gdt64

; =============================================================================
; BSS: Page tables, stack, and multiboot info storage
; =============================================================================
section .bss
align 4096

pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pd_table:
    resb 4096

stack_bottom:
    resb 65536              ; 64 KiB kernel stack
stack_top:

mb2_magic:
    resd 1
mb2_info:
    resq 1
