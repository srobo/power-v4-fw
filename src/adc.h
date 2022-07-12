#pragma once

#include <stdint.h>

#define TEMP_SENSE_CHANNEL 15

void adc_init(void);

uint16_t get_adc_measurement(uint8_t channel);

void read_next_current_phase(uint8_t phase);

int16_t adc_to_temp(uint16_t adc_val);
uint16_t adc_to_current(uint16_t adc_val);
