#pragma once

#include <stdint.h>

void battery_init(void);

uint16_t battery_current(void);
uint16_t battery_voltage(void);

uint16_t f_v(void);
uint16_t f_i(void);
