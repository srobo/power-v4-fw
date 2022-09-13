#pragma once

#include <stdbool.h>
#include <libopencm3/stm32/gpio.h>

#define FAN_PORT GPIOB
#define FAN_PIN GPIO1

void fan_init(void);

void fan_enable(const bool enable);
bool fan_running(void);
