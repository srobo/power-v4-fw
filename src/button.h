#pragma once

#include <stdbool.h>
#include <libopencm3/stm32/gpio.h>

#define INT_BTN_PORT GPIOC
#define INT_BTN_PIN GPIO14
#define EXT_BTN_PORT GPIOC
#define EXT_BTN_PIN GPIO15

extern bool button_pressed;

void button_init(void);

bool button_int_read(void);
bool button_ext_read(void);
