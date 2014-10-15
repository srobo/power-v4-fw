#pragma once

#include <stdint.h>

void battery_init(void);
void battery_poll(void);

uint16_t read_battery_voltage(void);
uint32_t read_battery_current(void);

// Should be considered internal

uint16_t f_vshunt(void);
uint16_t f_vbus(void);
