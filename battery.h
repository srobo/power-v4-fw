#pragma once

#include <stdint.h>

void battery_init(void);
void battery_poll(void);

uint16_t battery_vshunt(void);
uint16_t battery_vbus(void);
uint32_t battery_current(void);

uint16_t f_vshunt(void);
uint16_t f_vbus(void);
