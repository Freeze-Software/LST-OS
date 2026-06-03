#include "../display.h"

static int mouse_x = 0;
static int mouse_y = 0;
static uint32_t cursor_bg[16 * 16];

static const uint16_t cursor_shape[16] = {
    0b1000000000000000,
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111111110000000,
    0b1111111111000000,
    0b1111100000000000,
    0b1101100000000000,
    0b1000110000000000,
    0b0000110000000000,
    0b0000011000000000,
    0b0000011000000000,
};

void draw_cursor(int x, int y) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            if (cursor_shape[row] & (1 << (15 - col))) {
                draw_pixel(x + col, y + row, 0xFFFFFF);
            }
        }
    }
}

void save_cursor_bg(int x, int y) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            cursor_bg[row * 16 + col] = screen.buffer[(y + row) * (screen.pitch / 4) + (x + col)];
        }
    }
}

void restore_cursor_bg(int x, int y) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            screen.buffer[(y + row) * (screen.pitch / 4) + (x + col)] = cursor_bg[row * 16 + col];
        }
    }
}

void update_cursor(int new_x, int new_y) {
    int width, height;
    get_screen_res(&width, &height);
    if (new_x < 0) new_x = 0;
    if (new_y < 0) new_y = 0;
    if (new_x > width - 16)  new_x = width - 16;
    if (new_y > height - 16) new_y = height - 16;
    restore_cursor_bg(mouse_x, mouse_y);
    mouse_x = new_x;
    mouse_y = new_y;
    save_cursor_bg(mouse_x, mouse_y);
    draw_cursor(mouse_x, mouse_y);
}
