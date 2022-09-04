#pragma once
#include "i2c.h"

// Uses 30 nop commands for approximately 36 clocks per loop
#define delay(ms) do { \
    for (int i = 0; i < ms * 2000; i++) \
        __asm__( \
            "nop;nop;nop;nop;nop;" \
            "nop;nop;nop;nop;nop;" \
            "nop;nop;nop;nop;nop;" \
            "nop;nop;nop;nop;nop;" \
            "nop;nop;nop;nop;nop;" \
            "nop;nop;nop;nop;nop;"); \
    } while(0)

#define SERIALNUM_BOOTLOADER_LOC 0x08001FE0
#define REENTER_BOOTLOADER_RENDEZVOUS 0x08001FFC
#define BOARD_NAME_SHORT "PBv4B"

#define FAN_THRESHOLD 40
#define BRAIN_OUTPUT OUT_L2

extern volatile INA219_meas_t battery;
extern volatile INA219_meas_t reg_5v;
extern volatile int16_t board_temp;

extern volatile bool int_button_pressed;
extern volatile bool ext_button_pressed;

extern volatile uint8_t ADC_OVERCURRENT_DELAY;
extern volatile uint8_t BATT_OVERCURRENT_DELAY;
extern volatile uint8_t REG_OVERCURRENT_DELAY;
extern volatile uint8_t UVLO_DELAY;
