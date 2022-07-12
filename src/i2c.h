#pragma once

#include <stdint.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#define I_SENSE_LSB 0.001
#define I_CAL_VAL(I_SHUNT_RES) ((uint16_t)(0.04096/(I_SHUNT_RES * I_SENSE_LSB)))

typedef struct {
    int16_t voltage;
    int16_t current;
    bool success;
} INA219_meas_t;

void i2c_init(void);

void i2c_start_message(uint8_t addr);
void i2c_stop_message(void);

void i2c_send_byte(char c);
bool i2c_recv_bytes(uint8_t addr, uint8_t* buf, uint8_t len);

#define BATTERY_SENSE_ADDR 0x40
#define REG_SENSE_ADDR 0x41
void init_i2c_sensors();
void init_current_sense(uint8_t addr, uint16_t cal_val);
INA219_meas_t measure_current_sense(uint8_t addr);

void reset_i2c_watchdog(void);

extern volatile bool i2c_timed_out;
