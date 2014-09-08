#include "button.h"
#include <libopencm3/stm32/gpio.h>

#define INT_PORT GPIOC
#define INT_PIN GPIO14
#define EXT_PORT GPIOC
#define EXT_PIN GPIO15

void button_init(void) {
	gpio_set(INT_PORT, INT_PIN); // Pull-up
	gpio_set_mode(INT_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, INT_PIN);
	gpio_set(EXT_PORT, EXT_PIN); // Pull-up
	gpio_set_mode(EXT_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, EXT_PIN);
}

bool button_int_read(void) { return gpio_get(INT_PORT, INT_PIN); }
bool button_ext_read(void) { return gpio_get(EXT_PORT, EXT_PIN); }
