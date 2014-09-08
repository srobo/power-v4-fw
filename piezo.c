#include "piezo.h"
#include <libopencm3/stm32/gpio.h>

#define PIEZO_PORT GPIOB
#define PIEZO_PIN GPIO0

void piezo_init(void) {
	gpio_clear(PIEZO_PORT, PIEZO_PIN);
	gpio_set_mode(PIEZO_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, PIEZO_PIN);
}

void piezo_toggle(void) {
	gpio_toggle(PIEZO_PORT, PIEZO_PIN);
}
