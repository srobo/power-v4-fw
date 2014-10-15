#include <stdbool.h>

#include "battery.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "led.h"

void battery_init(void) {
	// We poll the battery current sense / voltage sense to see whether
	// something is wrong. Reads that are too close together cause the
	// INA219 to croak. Therefore set up a timer to periodically trigger
	// reads.
	// The IC settles readings at 2KHz; enable a timer @ 4Khz and read
	// voltage / current every other tick.
	rcc_periph_clock_enable(RCC_TIM2);
	timer_reset(TIM2);
	timer_set_prescaler(TIM2, 1799); // 72Mhz -> 40Khz
	timer_set_period(TIM2, 10); // 10 ticks -> 4Khz
	nvic_enable_irq(NVIC_TIM2_IRQ);
	nvic_set_priority(NVIC_TIM2_IRQ, 2); // Less important
	timer_enable_update_event(TIM2);
	timer_enable_irq(TIM2, TIM_DIER_UIE);
	timer_enable_counter(TIM2);
}

uint16_t battery_vbus()
{
	uint16_t vbus = 0; // XXX jmorse
	// Lower 3 bits are status bits. Rest is the voltage, measured in units
	// of 4mV. So, mask the lower 3 bits, then shift down by one.
	vbus &= 0xFFF8;
	vbus >>= 1;
	return vbus;
}

static uint32_t battery_current()
{
	uint16_t vshunt = 0; // XXX jmorse

	// The measurement just taken is measured in 10uV units over the 500uO
	// resistor pair on the battery rail. I = V/R, and R being small,
	// multiply by 20 to give a figure measured in units of 1mA.
	uint32_t current = vshunt * 20;

	// Additionally, the current drawn is consistently reported as being
	// 800mA over reality; adjust this amount.
	if (current < 800) {
		// It's also super jittery, and might dip below this offset
		current = 1;
	} else {
		current -= 800;
	}

	return current;
}

volatile bool batt_do_read = false;
uint32_t batt_read_current = 0;
uint32_t batt_read_voltage = 0;
enum { BATT_READ_CURR, BATT_READ_VOLT } batt_read_state = BATT_READ_CURR;

void tim2_isr(void)
{
	batt_do_read = true;
	TIM_SR(TIM2) = 0; // Inexplicably does not reset
	return;
}

void battery_poll()
{
	bool do_read = false;

	nvic_disable_irq(NVIC_TIM2_IRQ);
	if (batt_do_read) {
		// First, reset the counter, so there's another 250uS til the
		// next read
		timer_set_counter(TIM2, 0);
		// Then reset the read-flag
		batt_do_read = false;

		do_read = true;
	}
	nvic_enable_irq(NVIC_TIM2_IRQ);

	if (!do_read)
		return;

	// We're clear to read.
	switch (batt_read_state) {
	case BATT_READ_CURR:
		batt_read_state = BATT_READ_VOLT;
		batt_read_current = battery_current();
		break;
	case BATT_READ_VOLT:
		batt_read_state = BATT_READ_CURR;
		batt_read_voltage = battery_vbus();
		break;
	}

	return;
}

// Upon read, return the most recently sampled result.
uint16_t read_battery_voltage()
{
	return batt_read_voltage;
}

uint32_t read_battery_current()
{
	return batt_read_current;
}
