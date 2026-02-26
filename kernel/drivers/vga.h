#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

/* ── VGA text mode dimensions ─────────────────────────────────────────────── */
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  ((uint16_t *)0xB8000)

/* ── Color constants ─────────────────────────────────────────────────────── */
typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,  /* Yellow */
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

/* ── Public API ───────────────────────────────────────────────────────────── */
void vga_init(void);
void vga_clear(void);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_putchar(char c);
void vga_puts(const char *str);
void vga_printf(const char *fmt, ...);
void vga_print_hex(uint64_t value);
void vga_print_dec(uint64_t value);
void vga_move_cursor(uint16_t x, uint16_t y);
void vga_get_cursor(uint16_t *x, uint16_t *y);
void vga_set_cell(uint16_t x, uint16_t y, char c, uint8_t color);
void vga_scroll(void);

/* Current color attribute byte */
uint8_t vga_current_color(void);

#endif /* VGA_H */
