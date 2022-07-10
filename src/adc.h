#pragma once

#include <stdint.h>

void adc_init(void);

uint16_t get_adc_measurement(uint8_t channel);

void read_next_current_phase(uint8_t phase);
