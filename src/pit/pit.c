#include "../kernel.h"

#define PIT_CMD 0x43
#define PIT_CHANNEL0 0x40

static volatile uint32_t g_pit_ticks = 0;
static uint32_t g_pit_hz = 100;

void pit_init(int hz) {
    if (hz <= 0) {
        hz = 100;
    }

    g_pit_hz = (uint32_t)hz;
    g_pit_ticks = 0;

    int divisor = 1193182 / hz;

    outb(PIT_CMD, 0x36);

    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

void pit_on_tick(void) {
    g_pit_ticks++;
}

uint32_t pit_get_ticks(void) {
    return g_pit_ticks;
}

uint32_t pit_get_hz(void) {
    return g_pit_hz;
}
