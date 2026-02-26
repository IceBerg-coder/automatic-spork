/*
 * ============================================================================
 * SporkOS – 64-bit Operating System
 * Main Kernel Entry Point + Interactive Shell
 * ============================================================================
 */

#include "kernel.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "../libc/string.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Build timestamp ─────────────────────────────────────────────────────── */
#define OS_NAME     "SporkOS"
#define OS_VERSION  "1.0.0"
#define OS_ARCH     "x86_64"
#define OS_AUTHOR   "IceBerg-coder"

/* Multiboot2 magic number */
#define MULTIBOOT2_MAGIC 0xe85250d6

/* ── Shell state ─────────────────────────────────────────────────────────── */
#define SHELL_PROMPT    "spork> "
#define INPUT_BUF_SIZE  256
#define CMD_MAX_ARGS    16

static char  input_buf[INPUT_BUF_SIZE];
static int   input_len = 0;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void shell_run(void);
static void shell_execute(char *line);
static void shell_print_prompt(void);
static void draw_banner(void);

/* ── cmd_ handlers ────────────────────────────────────────────────────────── */
static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_about(int argc, char **argv);
static void cmd_color(int argc, char **argv);
static void cmd_halt(int argc, char **argv);
static void cmd_uptime(int argc, char **argv);
static void cmd_mem(int argc, char **argv);
static void cmd_lscpu(int argc, char **argv);

/* ── Command table ────────────────────────────────────────────────────────── */
typedef struct {
    const char *name;
    const char *description;
    void      (*handler)(int argc, char **argv);
} shell_cmd_t;

static const shell_cmd_t commands[] = {
    { "help",   "Show available commands",              cmd_help   },
    { "clear",  "Clear the screen",                     cmd_clear  },
    { "echo",   "Print text: echo <text...>",            cmd_echo   },
    { "about",  "Show OS information",                   cmd_about  },
    { "color",  "Set colors: color <fg> <bg> (0-15)",    cmd_color  },
    { "halt",   "Power off / halt the system",           cmd_halt   },
    { "uptime", "Show approximate system uptime ticks",  cmd_uptime },
    { "mem",    "Show memory layout",                    cmd_mem    },
    { "lscpu",  "Show CPUID information",                cmd_lscpu  },
    { NULL, NULL, NULL }
};

/* ── Tick counter incremented by IRQ0 (PIT) ──────────────────────────────── */
static volatile uint64_t system_ticks = 0;

static void timer_handler(interrupt_frame_t *frame)
{
    (void)frame;
    system_ticks++;
}

/* ── CPUID helper ─────────────────────────────────────────────────────────── */
static void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                  uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

/* ── kernel_main ──────────────────────────────────────────────────────────── */
void kernel_main(uint32_t mb2_magic, void *mb2_info)
{
    (void)mb2_info;

    /* Initialize subsystems */
    vga_init();
    gdt_init();
    idt_init();
    keyboard_init();

    /* Register PIT timer on IRQ0 */
    extern void irq_register_handler(uint8_t irq,
                                      void (*handler)(interrupt_frame_t *));
    irq_register_handler(0, timer_handler);

    /* Verify we were loaded by a Multiboot2-compliant bootloader */
    if (mb2_magic != MULTIBOOT2_MAGIC) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("\n[BOOT ERROR] Not loaded by a Multiboot2 bootloader!\n");
        for (;;) __asm__ volatile("hlt");
    }

    draw_banner();
    shell_run();
    __builtin_unreachable();
}

/* ── ASCII banner ─────────────────────────────────────────────────────────── */
static void draw_banner(void)
{
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts(
        " ____                   _     ___  ____\n"
        "/ ___| _ __   ___  _ __| | __/ _ \\/ ___|\n"
        "\\___ \\| '_ \\ / _ \\| '__| |/ / | | \\___ \\\n"
        " ___) | |_) | (_) | |  |   <| |_| |___) |\n"
        "|____/| .__/ \\___/|_|  |_|\\_\\\\___/|____/\n"
        "      |_|\n"
    );
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts(" Version " OS_VERSION " | " OS_ARCH " | by " OS_AUTHOR "\n");
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts(" Type 'help' for a list of available commands.\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/* ── Shell main loop ──────────────────────────────────────────────────────── */
static void shell_run(void)
{
    shell_print_prompt();

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            vga_putchar('\n');
            input_buf[input_len] = '\0';
            if (input_len > 0)
                shell_execute(input_buf);
            input_len = 0;
            shell_print_prompt();

        } else if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                vga_putchar('\b');
            }

        } else if (input_len < INPUT_BUF_SIZE - 1) {
            input_buf[input_len++] = c;
            vga_putchar(c);
        }
    }
}

static void shell_print_prompt(void)
{
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts(SHELL_PROMPT);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

/* ── Parse and execute a command line ────────────────────────────────────── */
static void shell_execute(char *line)
{
    /* Tokenize */
    char *argv[CMD_MAX_ARGS];
    int   argc = 0;

    char *token = strtok(line, " \t");
    while (token && argc < CMD_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    if (argc == 0) return;

    /* Search command table */
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].handler(argc, argv);
            return;
        }
    }

    /* Unknown command */
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("Unknown command: '");
    vga_puts(argv[0]);
    vga_puts("'. Type 'help' for a list.\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/* ============================================================================
 * Command Implementations
 * ============================================================================ */

static void cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("\nAvailable commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int i = 0; commands[i].name; i++) {
        vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
        vga_puts("  ");
        vga_puts(commands[i].name);
        /* Pad to 10 chars */
        int pad = 10 - (int)strlen(commands[i].name);
        while (pad-- > 0) vga_putchar(' ');
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_puts(commands[i].description);
        vga_putchar('\n');
    }
    vga_putchar('\n');
}

static void cmd_clear(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_clear();
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1) vga_putchar(' ');
        vga_puts(argv[i]);
    }
    vga_putchar('\n');
}

static void cmd_about(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("\n=== About " OS_NAME " ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("  Name         : " OS_NAME "\n");
    vga_puts("  Version      : " OS_VERSION "\n");
    vga_puts("  Architecture : " OS_ARCH "\n");
    vga_puts("  Author       : " OS_AUTHOR "\n");
    vga_puts("  Build date   : " __DATE__ " " __TIME__ "\n");
    vga_puts("  Bootloader   : GRUB2 (Multiboot2)\n");
    vga_puts("  Language     : C + x86_64 Assembly\n\n");
}

static void cmd_color(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: color <fg 0-15> <bg 0-15>\n");
        vga_puts("Colors: 0=black 1=blue 2=green 3=cyan 4=red 5=magenta\n");
        vga_puts("        6=brown 7=lgray 8=dgray 9=lblue 10=lgreen\n");
        vga_puts("        11=lcyan 12=lred 13=lmag 14=yellow 15=white\n");
        return;
    }
    int fg = atoi(argv[1]) & 0xF;
    int bg = atoi(argv[2]) & 0xF;
    vga_set_color((vga_color_t)fg, (vga_color_t)bg);
    vga_puts("Color changed.\n");
}

static void cmd_halt(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("\nHalting " OS_NAME "... Goodbye!\n");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

static void cmd_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint64_t ticks = system_ticks;
    uint64_t secs  = ticks / 100;   /* PIT typically fires ~100 Hz */
    vga_puts("System ticks : ");
    vga_print_dec(ticks);
    vga_puts("\nApprox secs  : ");
    vga_print_dec(secs);
    vga_putchar('\n');
}

static void cmd_mem(int argc, char **argv)
{
    (void)argc; (void)argv;
    extern uint8_t kernel_end[];     /* Defined in linker script */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("\n=== Memory Layout ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("  Kernel load base : 0x0000000000100000 (1 MiB)\n");
    vga_puts("  Kernel end       : ");
    vga_print_hex((uint64_t)(uintptr_t)kernel_end);
    vga_puts("\n  VGA framebuffer  : 0x00000000000B8000\n");
    vga_puts("  Stack (top)      : 0x0000000000110000 (approx)\n");
    vga_puts("  Page tables      : identity-mapped first 1 GiB\n\n");
}

static void cmd_lscpu(int argc, char **argv)
{
    (void)argc; (void)argv;

    uint32_t eax, ebx, ecx, edx;
    char vendor[13];

    /* Vendor string */
    cpuid(0, &eax, &ebx, &ecx, &edx);
    ((uint32_t *)vendor)[0] = ebx;
    ((uint32_t *)vendor)[1] = edx;
    ((uint32_t *)vendor)[2] = ecx;
    vendor[12] = '\0';

    /* Brand string (leaves 0x80000002–0x80000004) */
    char brand[49];
    memset(brand, 0, sizeof(brand));
    for (int i = 0; i < 3; i++) {
        cpuid(0x80000002 + (uint32_t)i,
              (uint32_t *)(brand + i * 16 + 0),
              (uint32_t *)(brand + i * 16 + 4),
              (uint32_t *)(brand + i * 16 + 8),
              (uint32_t *)(brand + i * 16 + 12));
    }
    brand[48] = '\0';

    /* Feature flags */
    cpuid(1, &eax, &ebx, &ecx, &edx);
    uint8_t stepping = eax & 0xF;
    uint8_t model    = (eax >> 4) & 0xF;
    uint8_t family   = (eax >> 8) & 0xF;

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("\n=== CPU Information ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("  Vendor  : "); vga_puts(vendor); vga_putchar('\n');
    vga_puts("  Brand   : "); vga_puts(brand);  vga_putchar('\n');
    vga_puts("  Family  : "); vga_print_dec(family);   vga_putchar('\n');
    vga_puts("  Model   : "); vga_print_dec(model);    vga_putchar('\n');
    vga_puts("  Stepping: "); vga_print_dec(stepping); vga_putchar('\n');

    vga_puts("  Features: ");
    if (edx & (1 << 0))  vga_puts("FPU ");
    if (edx & (1 << 4))  vga_puts("TSC ");
    if (edx & (1 << 6))  vga_puts("PAE ");
    if (edx & (1 << 9))  vga_puts("APIC ");
    if (edx & (1 << 23)) vga_puts("MMX ");
    if (edx & (1 << 25)) vga_puts("SSE ");
    if (edx & (1 << 26)) vga_puts("SSE2 ");
    if (ecx & (1 << 0))  vga_puts("SSE3 ");
    if (ecx & (1 << 28)) vga_puts("AVX ");
    vga_putchar('\n');

    vga_putchar('\n');
}
