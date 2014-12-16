#include <stdbool.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "clock.h"

static volatile bool clock_fired = false;
static uint8_t clock_count = 0;

/* Drama: There are only two basic timers on the STM32F103 we're using. As a
 * result, to conserve them, the 4Khz timer used for battery signalling is
 * re-used and divided here to provide a 1Khz clock. */

void
clock_isr()
{

	clock_count++;
	if (clock_count == 4) {
		clock_count = 0;
		clock_fired = true;
	}

	return;
}

bool
clock_tick(void)
{
	bool result = false;

	// Disable TIM2 (battery timer) while reading clock_fired
	nvic_disable_irq(NVIC_TIM2_IRQ);
	if (clock_fired) {
		result = true;
		clock_fired = false;
	}
	nvic_enable_irq(NVIC_TIM2_IRQ);

	return result;
}
