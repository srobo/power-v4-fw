#include <stdbool.h>

#include "button.h"
#include "smps.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#define INT_PORT GPIOC
#define INT_PIN GPIO14
#define EXT_PORT GPIOC
#define EXT_PIN GPIO15

#define delay(x) do { for (int i = 0; i < x * 1000; i++) \
                          __asm__("nop"); \
                    } while(0)

void button_init(void) {
	gpio_set(INT_PORT, INT_PIN); // Pull-up
	gpio_set_mode(INT_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, INT_PIN);
	gpio_set(EXT_PORT, EXT_PIN); // Pull-up
	gpio_set_mode(EXT_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, EXT_PIN);
}

bool button_int_read(void) { return gpio_get(INT_PORT, INT_PIN); }
bool button_ext_read(void) { return gpio_get(EXT_PORT, EXT_PIN); }


bool force_bootloader() __attribute__((section(".bootloader")));

bool force_bootloader()
{

	// Enable GPIOC
	rcc_periph_clock_enable(RCC_GPIOC);

	// HACK TIME: When the bootloader runs, we need to turn the 5v rail smps on.
	// This is because if we stick in the bootloader, the odroid needs to be
	// brought up so that it can actually flash the power board. If we didn't,
	// we'd be stuck in a state where the power board is on, nothing else is,
	// thus nothing can cause the power board to make progress.
	smps_on_boot();

	// Function to check whether we should force the bootloader.
	// Specifically, do that if the start button is pressed on startup
	gpio_set(INT_PORT, INT_PIN); // Pull-up
	gpio_set_mode(INT_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, INT_PIN);
	gpio_set(EXT_PORT, EXT_PIN); // Pull-up
	gpio_set_mode(EXT_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, EXT_PIN);

	// Rise time
	delay(10);

	if (!button_int_read() && !button_ext_read())
		return true;
	return false;
}

bool button_pressed(void)
{
	return !button_int_read() || !button_ext_read();
}
