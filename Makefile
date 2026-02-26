############################################################################################
# SporkOS Build System
# Targets:
#   all     – build SporkOS kernel ELF + bootable ISO
#   iso     – create GRUB2 bootable ISO (requires grub-mkrescue + xorriso)
#   clean   – remove all build artefacts
#   run     – boot in QEMU (requires qemu-system-x86_64)
#   debug   – boot in QEMU with GDB server on port 1234
############################################################################################

# ── Toolchain ──────────────────────────────────────────────────────────────
AS      := nasm
CC      := gcc
LD      := ld
GRUB    := grub-mkrescue
QEMU    := qemu-system-x86_64

# ── Flags ──────────────────────────────────────────────────────────────────
ASFLAGS := -f elf64

CFLAGS  := -std=c11                  \
            -ffreestanding            \
            -fno-stack-protector      \
            -fno-pic                  \
            -fno-pie                  \
            -mno-red-zone             \
            -mno-mmx                  \
            -mno-sse                  \
            -mno-sse2                 \
            -Wall -Wextra             \
            -O2                       \
            -Iinclude                 \
            -Ilibc

LDFLAGS := -T linker.ld              \
            -nostdlib                  \
            -z max-page-size=4096

QEMUFLAGS := -m 256M                 \
              -vga std                \
              -serial stdio           \
              -no-reboot              \
              -no-shutdown

# ── Directories ────────────────────────────────────────────────────────────
BUILD   := build
ISO_DIR := $(BUILD)/iso
BOOT_DIR := $(ISO_DIR)/boot
GRUB_DIR := $(BOOT_DIR)/grub

KERNEL_ELF  := $(BUILD)/sporkos.elf
KERNEL_ISO  := $(BUILD)/sporkos.iso

# ── Sources ────────────────────────────────────────────────────────────────
ASM_SRCS := boot/boot.asm                       \
             kernel/arch/x86_64/isr.asm

C_SRCS   := kernel/kernel.c                     \
             kernel/arch/x86_64/gdt.c            \
             kernel/arch/x86_64/idt.c            \
             kernel/drivers/vga.c                \
             kernel/drivers/keyboard.c           \
             libc/string.c

# ── Object files ───────────────────────────────────────────────────────────
ASM_OBJS := $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))
C_OBJS   := $(patsubst %.c,   $(BUILD)/%.o, $(C_SRCS))
ALL_OBJS := $(ASM_OBJS) $(C_OBJS)

# ── Default target ─────────────────────────────────────────────────────────
.PHONY: all iso clean run debug

all: $(KERNEL_ELF)

# ── Link kernel ────────────────────────────────────────────────────────────
$(KERNEL_ELF): $(ALL_OBJS) linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)
	@echo "[LD]  $@"
	@echo "[OK]  Kernel ELF built successfully."

# ── Assemble .asm files ────────────────────────────────────────────────────
$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<
	@echo "[AS]  $<"

# ── Compile .c files ───────────────────────────────────────────────────────
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo "[CC]  $<"

# ── Build bootable ISO ─────────────────────────────────────────────────────
iso: $(KERNEL_ELF)
	@mkdir -p $(GRUB_DIR)
	@cp $(KERNEL_ELF) $(BOOT_DIR)/sporkos.elf
	@cp boot/grub.cfg  $(GRUB_DIR)/grub.cfg
	$(GRUB) -o $(KERNEL_ISO) $(ISO_DIR)
	@echo "[ISO] $(KERNEL_ISO) ready."

# ── Run in QEMU from ISO ───────────────────────────────────────────────────
run: iso
	$(QEMU) $(QEMUFLAGS) -cdrom $(KERNEL_ISO)

# ── Run directly from ELF (not available for Multiboot2; use 'make run' instead) ──
run-elf:
	@echo ""
	@echo "NOTE: SporkOS uses Multiboot2 and requires a real bootloader."
	@echo "      QEMU's -kernel flag only supports Linux/PVH ELF format."
	@echo "      Use 'make run' (boots from GRUB ISO) instead."
	@echo ""

# ── GDB debug session ─────────────────────────────────────────────────────
debug: iso
	$(QEMU) $(QEMUFLAGS) -cdrom $(KERNEL_ISO) -boot d -s -S &
	gdb -ex "target remote localhost:1234"  \
	    -ex "symbol-file $(KERNEL_ELF)"     \
	    -ex "break kernel_main"             \
	    -ex "continue"

# ── Clean ──────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD)
	@echo "[CLEAN] Build directory removed."
