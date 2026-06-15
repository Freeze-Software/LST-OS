#include <stdint.h>

void pit_init(int hz);
void pit_on_tick(void);
uint32_t pit_get_ticks(void);
uint32_t pit_get_hz(void);
