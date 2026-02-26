# SporkOS

A unique, from-scratch 64-bit operating system for x86-64 written in C and x86_64 Assembly.

```
 ____                   _     ___  ____
/ ___| _ __   ___  _ __| | __/ _ \/ ___|
\___ \| '_ \ / _ \| '__| |/ / | | \___ \
 ___) | |_) | (_) | |  |   <| |_| |___) |
|____/| .__/ \___/|_|  |_|\_\\___/|____/
      |_|
  Version 1.0.0 | x86_64 | by IceBerg-coder
```

---

## Architecture

SporkOS boots via **GRUB2 Multiboot2**, transitions from **32-bit protected mode to 64-bit long mode**
in hand-written assembly, then jumps into a C kernel.

### Boot sequence

```
BIOS/UEFI
  └── GRUB2 (Multiboot2)
        └── boot/boot.asm  [32-bit]
              ├── Verify CPUID + Long Mode support
              ├── Build 4-level identity-mapped page tables (2 MiB huge pages)
              ├── Enable PAE + Long Mode (IA32_EFER.LME) + Paging
              └── Far-jump to 64-bit code
                    └── kernel/kernel.c :: kernel_main()
                          ├── gdt_init()      — load 64-bit GDT + TSS
                          ├── idt_init()      — install 256-entry IDT, remap PIC
                          ├── keyboard_init() — IRQ1 PS/2 keyboard driver
                          └── shell_run()     — interactive shell loop
```

---

## Project Structure

```
SporkOS/
├── boot/
│   ├── boot.asm            Multiboot2 header + 32-bit to 64-bit long mode transition
│   └── grub.cfg            GRUB2 menu configuration
├── kernel/
│   ├── kernel.c            Kernel entry, subsystem init, interactive shell + 9 commands
│   ├── kernel.h            Common kernel types and port I/O inlines
│   ├── arch/x86_64/
│   │   ├── gdt.c / gdt.h   64-bit GDT (null, kernel code/data, user code/data, TSS)
│   │   ├── idt.c / idt.h   256-entry IDT + PIC remapping + IRQ dispatch table
│   │   └── isr.asm         Exception (0-31) + IRQ (32-47) stubs; gdt_flush / tss_flush
│   └── drivers/
│       ├── vga.c / vga.h   VGA text-mode driver (80x25, scrolling, 16 colors, HW cursor)
│       └── keyboard.c /
│           keyboard.h      PS/2 keyboard driver (US QWERTY, Shift/Caps/Ctrl, ring buffer)
├── libc/
│   └── string.c / .h       Freestanding C string + memory library
├── include/
│   ├── stdint.h            Freestanding integer types
│   ├── stddef.h            size_t, ptrdiff_t, NULL
│   ├── stdbool.h           bool / true / false
│   └── stdarg.h            va_list (wraps GCC builtins)
├── linker.ld               Kernel linker script (loads at 1 MiB)
└── Makefile                Build system
```

---

## Features

| Subsystem | Details |
|---|---|
| **Bootloader** | Multiboot2; CPUID + long-mode checks; 4-level paging (1 GiB identity map) |
| **GDT** | null, kernel code/data, user code/data, 64-bit TSS loaded with lgdt + far-return flush |
| **IDT** | 256 entries; exceptions 0-31 with error-code handling; hardware IRQs 32-47; 8259 PIC |
| **VGA driver** | 80x25 text mode; 16 colors; hardware cursor; scrolling; vga_printf |
| **Keyboard** | PS/2 IRQ1; US QWERTY scan-code table; Shift / Caps Lock / Ctrl; 256-byte ring buffer |
| **Shell** | Interactive spork> prompt with backspace, tokenizer, and 9 built-in commands |
| **Libc** | Freestanding memcpy/set/cmp, strlen/strcmp/strtok/strstr, atoi/atol |

### Shell commands

| Command | Description |
|---|---|
| `help` | List all commands |
| `clear` | Clear the screen |
| `echo <text>` | Print text |
| `about` | Show OS version, build date, toolchain |
| `color <fg> <bg>` | Change terminal colors (0-15) |
| `uptime` | Show system tick counter (PIT IRQ0) |
| `mem` | Show kernel memory layout |
| `lscpu` | Show CPU vendor, brand, family/model, feature flags |
| `halt` | Halt the machine |

---

## Building

### Prerequisites

```bash
# Ubuntu / Debian
sudo apt-get install nasm gcc binutils grub-pc-bin grub-common xorriso qemu-system-x86
```

### Targets

```bash
make all        # Build kernel ELF  ->  build/sporkos.elf
make iso        # Build bootable ISO ->  build/sporkos.iso
make run        # Build ISO and boot in QEMU (recommended)
make run-elf    # Prints a note: use 'make run' instead (Multiboot2 needs GRUB)
make debug      # Boot ISO with GDB server on port 1234
make clean      # Remove build/
```

### Quick start

```bash
git clone https://github.com/IceBerg-coder/automatic-spork
cd automatic-spork
make run-elf    # requires qemu-system-x86_64
```

---

## Build artifacts

| File | Size |
|---|---|
| `build/sporkos.elf` | ~35 KiB |
| `build/sporkos.iso` | ~5 MiB (GRUB2 + kernel) |