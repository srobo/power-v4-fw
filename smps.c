#include "smps.h"
#include <libopencm3/stm32/gpio.h>

#define EN_PORT GPIOB
#define EN_PIN GPIO5
#define TRIM_PORT GPIOC
#define TRIM_PIN GPIO12

void smps_init(void) {
	gpio_clear(EN_PORT, EN_PIN);
	gpio_set_mode(EN_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, EN_PIN);
	gpio_set(TRIM_PORT, TRIM_PIN);
	gpio_set_mode(TRIM_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, TRIM_PIN);
}

void smps_on(void) {
	gpio_set(EN_PORT, EN_PIN);
}

void smps_off(void) {
	gpio_clear(EN_PORT, EN_PIN);
}

void smps_boost_on(void) {
	gpio_clear(TRIM_PORT, TRIM_PIN);
}

void smps_boost_off(void) {
	gpio_set(TRIM_PORT, TRIM_PIN);
}

void smps_on_boot(void) __attribute__((section(".bootloader")));

void smps_on_boot(void) {
	// Turn SMPS on in the context of the bootloader
	gpio_clear(EN_PORT, EN_PIN);
	gpio_set_mode(EN_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, EN_PIN);
	gpio_set(EN_PORT, EN_PIN);
}
