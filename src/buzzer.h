#pragma once
#include <stdint.h>
#include <stdbool.h>

void buzzer_init(void);
void buzzer_note(uint16_t freq, uint32_t duration);
void buzzer_stop(void);
bool buzzer_running(void);
void buzzer_tick(void);
