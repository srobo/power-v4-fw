#include "led.h"

void led_init(void) {
	led_clear(LED_RUN);
	led_clear(LED_ERROR);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_10_MHZ,
	              GPIO_CNF_OUTPUT_PUSHPULL, LED_RUN);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_10_MHZ,
	              GPIO_CNF_OUTPUT_PUSHPULL, LED_ERROR);
	gpio_set_mode(GPIOD, GPIO_MODE_OUTPUT_10_MHZ,
	              GPIO_CNF_OUTPUT_PUSHPULL, GPIO2);
}
