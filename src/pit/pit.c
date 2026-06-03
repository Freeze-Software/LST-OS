#include "../kernel.h"

#define PIT_CMD 0x43
#define PIT_CHANNEL0 0x40

void pit_init(int hz) {
    int divisor = 1193182 / hz;

    outb(PIT_CMD, 0x36);

    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}
