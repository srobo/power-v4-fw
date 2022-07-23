#pragma once
#include "i2c.h"

#define FAN_THRESHOLD 40

extern INA219_meas_t battery;
extern INA219_meas_t reg_5v;
extern int16_t board_temp;
