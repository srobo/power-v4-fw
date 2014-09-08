#include "pswitch.h"
#include <libopencm3/stm32/gpio.h>

#define PSWITCH_PORT GPIOC
#define PSWITCH_PIN GPIO13

void pswitch_init(void) {
	gpio_set(PSWITCH_PORT, PSWITCH_PIN); // Pull-up
	gpio_set_mode(PSWITCH_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, PSWITCH_PIN);
}

bool pswitch_read(void) {
	return gpio_get(PSWITCH_PORT, PSWITCH_PIN);
}
