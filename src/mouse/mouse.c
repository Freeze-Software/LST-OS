#include <stdint.h>
#include "mouse.h"
#include "render.h"
#include "../display.h"

#define PS2_TIMEOUT 1000000u

extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t val);

ps2mouse_state_t mouse;

uint8_t mouse_cycle = 0;
uint8_t mouse_packet[3];
int mouse_available = 0;

static void mouse_process_byte(uint8_t data) {
    if (mouse_cycle == 0 && !(data & 0x08)) {
        return;
    }

    mouse_packet[mouse_cycle++] = data;
    if (mouse_cycle < 3) {
        return;
    }
    mouse_cycle = 0;

    {
        int dx = (int8_t)mouse_packet[1];
        int dy = (int8_t)mouse_packet[2];
        mouse.x += dx;
        mouse.y -= dy;
    }

    mouse.left   = mouse_packet[0] & 1;
    mouse.right  = mouse_packet[0] & 2;
    mouse.middle = mouse_packet[0] & 4;
    update_cursor(mouse.x, mouse.y);
}

static int ps2mouse_wait_input(void) {
    uint32_t timeout = PS2_TIMEOUT;
    while ((inb(0x64) & 2) && timeout > 0) {
        timeout--;
    }
    return timeout != 0;
}

static int ps2mouse_wait_output(void) {
    uint32_t timeout = PS2_TIMEOUT;
    while (!(inb(0x64) & 1) && timeout > 0) {
        timeout--;
    }
    return timeout != 0;
}


void mouse_irq_handler() {
    if (!(inb(0x64) & 0x20))
        return;

    uint8_t data = inb(0x60);
    mouse_process_byte(data);
}

void mouse_poll(void) {
    if (!mouse_available) {
        return;
    }

    while (inb(0x64) & 1) {
        uint8_t status = inb(0x64);
        if (!(status & 0x20)) {
            break;
        }
        mouse_process_byte(inb(0x60));
    }
}

static int ps2mouse_write(uint8_t data) {
    if (!ps2mouse_wait_input()) {
        return 0;
    }
    outb(0x64, 0xD4);
    if (!ps2mouse_wait_input()) {
        return 0;
    }
    outb(0x60, data);
    return 1;
}

static int ps2mouse_read(uint8_t *value) {
    if (!ps2mouse_wait_output()) {
        return 0;
    }
    *value = inb(0x60);
    return 1;
}

static void ps2mouse_enable_keyboard_only(void) {
    if (ps2mouse_wait_input()) {
        outb(0x64, 0xAE);
    }
}

static void ps2mouse_reset_state(void) {
    mouse_available = 0;
    mouse_cycle = 0;
    mouse.x = 0;
    mouse.y = 0;
    mouse.left = 0;
    mouse.right = 0;
    mouse.middle = 0;
}

void ps2mouse_init() {
    uint8_t config;
    uint8_t data;

    ps2mouse_reset_state();

    if (!ps2mouse_wait_input()) {
        return;
    }
    outb(0x64, 0xAD); // disable keyboard
    if (!ps2mouse_wait_input()) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    outb(0x64, 0xA7); // disable mouse
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    if (!ps2mouse_wait_input()) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    outb(0x64, 0x20);
    if (!ps2mouse_read(&config)) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    config |=  (1 << 0); // enable keyboard IRQ
    config |=  (1 << 1); // enable mouse IRQ
    config &= ~(1 << 4); // enable keyboard clock
    config &= ~(1 << 5); // enable mouse clock
    if (!ps2mouse_wait_input()) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    outb(0x64, 0x60);
    if (!ps2mouse_wait_input()) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    outb(0x60, config);
    if (!ps2mouse_wait_input()) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    outb(0x64, 0xA8);
    if (!ps2mouse_write(0xFF)) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    if (!ps2mouse_read(&data) || !ps2mouse_read(&data) || !ps2mouse_read(&data)) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    if (!ps2mouse_write(0xF6) || !ps2mouse_read(&data)) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    if (!ps2mouse_write(0xF4) || !ps2mouse_read(&data)) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    if (!ps2mouse_wait_input()) {
        ps2mouse_enable_keyboard_only();
        return;
    }
    outb(0x64, 0xAE);
    mouse_available = 1;
    update_cursor(mouse.x, mouse.y);
}
