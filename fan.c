#include "fan.h"
#include <libopencm3/stm32/gpio.h>

#define FAN_PORT GPIOB
#define FAN_PIN GPIO1

void fan_init(void) {
	gpio_clear(FAN_PORT, FAN_PIN);
	gpio_set_mode(FAN_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, FAN_PIN);
}

void fan_on(void) {
	gpio_set(FAN_PORT, FAN_PIN);
}

void fan_off(void) {
	gpio_clear(FAN_PORT, FAN_PIN);
}
