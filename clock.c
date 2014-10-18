#include <stdbool.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "clock.h"

static volatile bool clock_fired = false;

void
clock_init()
{

	rcc_periph_clock_enable(RCC_TIM3);
	timer_reset(TIM3);
	timer_set_prescaler(TIM3, 7200); // 72Mhz -> 10Khz
	timer_set_period(TIM3, 10); // 10 ticks -> 1Khz
	nvic_enable_irq(NVIC_TIM3_IRQ);
	nvic_set_priority(NVIC_TIM3_IRQ, 2); // Less important
	timer_enable_update_event(TIM3);
	timer_enable_irq(TIM3, TIM_DIER_UIE);
	timer_enable_counter(TIM3);
}

void
tim3_isr()
{

	clock_fired = true;
	TIM_SR(TIM3) = 0;
	return;
}

bool
clock_tick(void)
{
	bool result = false;

	nvic_disable_irq(NVIC_TIM3_IRQ);
	if (clock_fired) {
		result = true;
		clock_fired = false;
	}
	nvic_enable_irq(NVIC_TIM3_IRQ);

	return result;
}
