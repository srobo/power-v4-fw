#pragma once

#include <stdint.h>

void output_init(void);

void output_poll(void);

void output_on(uint8_t n);

void output_off(uint8_t n);

void output_stat_on(uint8_t n);

void output_stat_off(uint8_t n);

void current_sense_recvsamples(uint32_t samp1, uint32_t samp2);
void current_sense_poll();
uint32_t current_sense_read(int output);
