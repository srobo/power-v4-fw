#pragma once
#include "i2c.h"

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
#define BOARD_NAME_SHORT "PBv4B"

#define FAN_THRESHOLD 40
#define BRAIN_OUTPUT OUT_L2

extern INA219_meas_t battery;
extern INA219_meas_t reg_5v;
extern int16_t board_temp;

extern bool int_button_pressed;
extern bool ext_button_pressed;
