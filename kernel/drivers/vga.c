#include "vga.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

/* ── Internal state ──────────────────────────────────────────────────────── */
static uint16_t  cursor_x    = 0;
static uint16_t  cursor_y    = 0;
static uint8_t   color_attr  = 0;
static uint16_t *vga_buf     = VGA_MEMORY;

/* ── Build a VGA entry word ──────────────────────────────────────────────── */
static inline uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

/* ── Update the blinking hardware cursor ──────────────────────────────────── */
static void update_hw_cursor(void)
{
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    /* VGA CRTC: cursor location high/low */
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x0F), "Nd"((uint16_t)0x3D4));
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)(pos & 0xFF)), "Nd"((uint16_t)0x3D5));
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0x0E), "Nd"((uint16_t)0x3D4));
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)((pos >> 8) & 0xFF)), "Nd"((uint16_t)0x3D5));
}

/* ── Scroll the screen up one line ────────────────────────────────────────── */
void vga_scroll(void)
{
    /* Move every row up by one */
    for (uint16_t y = 1; y < VGA_HEIGHT; y++)
        for (uint16_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[(y - 1) * VGA_WIDTH + x] = vga_buf[y * VGA_WIDTH + x];

    /* Clear the last row */
    for (uint16_t x = 0; x < VGA_WIDTH; x++)
        vga_buf[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color_attr);

    if (cursor_y > 0)
        cursor_y--;
}

/* ── vga_init ─────────────────────────────────────────────────────────────── */
void vga_init(void)
{
    color_attr = (VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_GREEN;
    vga_buf    = VGA_MEMORY;
    vga_clear();
}

/* ── vga_clear ────────────────────────────────────────────────────────────── */
void vga_clear(void)
{
    for (uint16_t y = 0; y < VGA_HEIGHT; y++)
        for (uint16_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = vga_entry(' ', color_attr);
    cursor_x = 0;
    cursor_y = 0;
    update_hw_cursor();
}

/* ── vga_set_color ────────────────────────────────────────────────────────── */
void vga_set_color(vga_color_t fg, vga_color_t bg)
{
    color_attr = (uint8_t)((bg << 4) | (fg & 0x0F));
}

uint8_t vga_current_color(void) { return color_attr; }

/* ── vga_set_cell ─────────────────────────────────────────────────────────── */
void vga_set_cell(uint16_t x, uint16_t y, char c, uint8_t color)
{
    if (x < VGA_WIDTH && y < VGA_HEIGHT)
        vga_buf[y * VGA_WIDTH + x] = vga_entry((unsigned char)c, color);
}

/* ── vga_putchar ──────────────────────────────────────────────────────────── */
void vga_putchar(char c)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        vga_buf[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', color_attr);
    } else if (c == '\t') {
        /* Advance to next 8-char tab stop */
        cursor_x = (uint16_t)((cursor_x + 8) & ~7);
    } else {
        vga_buf[cursor_y * VGA_WIDTH + cursor_x] = vga_entry((unsigned char)c, color_attr);
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= VGA_HEIGHT)
        vga_scroll();

    update_hw_cursor();
}

/* ── vga_puts ─────────────────────────────────────────────────────────────── */
void vga_puts(const char *str)
{
    while (*str)
        vga_putchar(*str++);
}

/* ── vga_print_hex ────────────────────────────────────────────────────────── */
void vga_print_hex(uint64_t value)
{
    const char *hex = "0123456789ABCDEF";
    char buf[19];   /* "0x" + 16 hex digits + NUL */
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++)
        buf[2 + i] = hex[(value >> (60 - i * 4)) & 0xF];
    buf[18] = '\0';
    vga_puts(buf);
}

/* ── vga_print_dec ────────────────────────────────────────────────────────── */
void vga_print_dec(uint64_t value)
{
    if (value == 0) { vga_putchar('0'); return; }
    char buf[21];
    int  i = 20;
    buf[i] = '\0';
    while (value > 0 && i > 0) {
        buf[--i] = (char)('0' + (value % 10));
        value /= 10;
    }
    vga_puts(buf + i);
}

/* ── vga_printf (minimal: %s %c %d %u %x %p) ─────────────────────────────── */
void vga_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') { vga_putchar(*fmt); continue; }
        fmt++;
        switch (*fmt) {
            case 'c': vga_putchar((char)va_arg(args, int));   break;
            case 's': vga_puts(va_arg(args, const char *));   break;
            case 'd': {
                int v = va_arg(args, int);
                if (v < 0) { vga_putchar('-'); vga_print_dec((uint64_t)-v); }
                else        { vga_print_dec((uint64_t)v); }
                break;
            }
            case 'u': vga_print_dec(va_arg(args, unsigned));  break;
            case 'x': {
                uint64_t v = va_arg(args, uint64_t);
                vga_print_hex(v);
                break;
            }
            case 'p': {
                uint64_t v = (uint64_t)va_arg(args, void *);
                vga_print_hex(v);
                break;
            }
            case '%': vga_putchar('%'); break;
            default:  vga_putchar('%'); vga_putchar(*fmt); break;
        }
    }

    va_end(args);
}

/* ── vga_move_cursor ──────────────────────────────────────────────────────── */
void vga_move_cursor(uint16_t x, uint16_t y)
{
    cursor_x = (x < VGA_WIDTH)  ? x : VGA_WIDTH  - 1;
    cursor_y = (y < VGA_HEIGHT) ? y : VGA_HEIGHT - 1;
    update_hw_cursor();
}

void vga_get_cursor(uint16_t *x, uint16_t *y)
{
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}
