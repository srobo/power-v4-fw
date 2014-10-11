#pragma once

#include <stdint.h>

void battery_init(void);

uint16_t battery_vshunt(void);
uint16_t battery_vbus(void);

uint16_t f_vshunt(void);
uint16_t f_vbus(void);
