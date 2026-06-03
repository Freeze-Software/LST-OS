#include "kernel.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)
#define VGA_COLOR_DEFAULT 0x2F

static size_t row = 0;
static size_t col = 0;
static uint8_t vga_color = VGA_COLOR_DEFAULT;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void vga_update_hardware_cursor(void) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void vga_scroll(void) {
    if (row < VGA_HEIGHT) {
        return;
    }

    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    uint16_t blank = ((uint16_t)vga_color << 8) | ' ';
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }

    row = VGA_HEIGHT - 1;
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
    vga_update_hardware_cursor();
}

void vga_clear(void) {
    uint16_t blank = ((uint16_t)vga_color << 8) | ' ';
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = blank;
        }
    }
    row = 0;
    col = 0;
    vga_update_hardware_cursor();
}

void vga_init(void) {
    vga_clear();
}

void vga_set_color(uint8_t color) {
    vga_color = color;
}

void vga_backspace(void) {
    if (col == 0) {
        if (row == 0) return;
        row--;
        col = VGA_WIDTH - 1;
    } else {
        col--;
    }
    VGA_MEMORY[row * VGA_WIDTH + col] = ((uint16_t)vga_color << 8) | ' ';
    vga_update_hardware_cursor();
}

void vga_putc(char c) {
    if (c == '\n') {
        col = 0;
        row++;
        vga_scroll();
        vga_update_hardware_cursor();
        return;
    }

    if (c == '\r') {
        col = 0;
        vga_update_hardware_cursor();
        return;
    }

    if (c == '\b') {
        vga_backspace();
        return;
    }

    if (c == '\t') {

        for (int i = 0; i < 4; i++) vga_putc(' ');
        return;
    }

    VGA_MEMORY[row * VGA_WIDTH + col] = ((uint16_t)vga_color << 8) | (uint8_t)c;
    col++;

    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
    }

    vga_scroll();
    vga_update_hardware_cursor();
}

void vga_write(const char *s) {
    while (*s) {
        vga_putc(*s++);
    }
}

void vga_write_hex(uint32_t val) {
    const char *hex = "0123456789ABCDEF";
    vga_write("0x");
    bool started = false;
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nib = (val >> i) & 0xF;
        if (nib || started || i == 0) {
            vga_putc(hex[nib]);
            started = true;
        }
    }
}

void vga_write_dec(uint32_t val) {
    char buf[11];
    int i = 0;
    if (val == 0) {
        vga_putc('0');
        return;
    }
    while (val > 0 && i < (int)sizeof(buf)-1) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i--) vga_putc(buf[i]);
}