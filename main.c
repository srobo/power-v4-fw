#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>

#include "led.h"
#include "cdcacm.h"
#include "output.h"
#include "fan.h"
#include "smps.h"
#include "piezo.h"
#include "button.h"
#include "battery.h"
#include "usart.h"
#include "pswitch.h"
#include "analogue.h"
#include "usb.h"
#include "i2c.h"
#include "clock.h"

#include "dfu-bootloader/usbdfu.h"

#define delay(x) do { for (int i = 0; i < x * 1000; i++) \
                          __asm__("nop"); \
                    } while(0)

void
init()
{

	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPAEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPBEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPCEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPDEN);

	usb_init();
	i2c_init();
	led_init();
	output_init();
	fan_init();
	smps_init();
	piezo_init();
	button_init();
	battery_init();
	usart_init();
	pswitch_init();
	analogue_init();
	clock_init();
}

void
shut_down_everything()
{
	unsigned int i;
	// In case of the battery being low, or drawing too much current,
	// turn off all our peripherals. This includes turning the fan off,
	// as that'll eventually knacker the battery by itself.
	for (i = 0; i < 6; i++)
		output_off(i);

	for (i = 0; i < 6; i++)
		output_stat_off(0);

	led_clear(LED_RUN);
	led_clear(LED_ERROR);
	led_clear_flat(); // Maybe configure later.

	smps_off();

	fan_off();

	return;
}

void
check_batt_undervolt()
{
	uint32_t voltage = read_battery_voltage();

	// Check if voltage is < 10.2V. Skip if it's zero, which means no
	// voltage sample has been read yet.
	// XXX watchdog / timer to detect too-long-since-sample condition
	if (voltage != 0 && voltage < 10200) {
		// The battery is low, or otherwise has massively drooped.
		// To avoid knackering it, turn everything off and blink the
		// charge light.
		shut_down_everything();

		while (1) {
			led_toggle_flat();
			delay(2000);
		}
	}
	return;
}

void
check_batt_current_limit()
{
	uint32_t current = read_battery_current();
	if (current >= 40000) {
		// Something is wrong.
		shut_down_everything();

		// At this point, whatever was overloading the battery is now
		// turned off, or the power board is ablaze. Sit in a loop
		// beeping, to let the user know something went wrong.
		//
		// In this mode, we do not want to knacker the battery either,
		// so continue to poll for low voltage.
		while (1) {
			check_batt_undervolt();

			for (unsigned int i = 0; i < 2000; i++) {
				piezo_toggle();
				delay(1);
			}

			check_batt_undervolt();

			delay(2000);
		}
	}
}

int
main()
{

	init();

	while (1) {
		// Do things
		usb_poll();
		current_sense_poll();
		i2c_poll();
		battery_poll();
		check_batt_current_limit();
		check_batt_undervolt();
		button_poll();

		if (re_enter_bootloader) {
			// Because spoons can't be used as forks, we have to
			// actually wait for the usb peripheral to complete
			// it's acknowledgement to dfu_detach
			delay(20);
			// Now reset USB
			usb_deinit();
			// Disable any irqs that there are. XXX this is likely
			// to get out of sync.
			nvic_disable_irq(NVIC_ADC1_2_IRQ);
			nvic_disable_irq(NVIC_TIM2_IRQ);
			// Call back into bootloader
			(*(void (**)())(REENTER_BOOTLOADER_RENDEZVOUS))();
		}
	}
}

// Configure application start address, put in section that'll be placed at
// the start of the non-bootloader firmware. The actual start address is
// libopencm3's reset handler, seeing how that's what copies .data into sram.
extern void *vector_table;
extern __attribute__((naked)) void reset_handler(void);
uint32_t app_start_address[2] __attribute__((section(".lolstartup"))) =
{
	(uint32_t)&vector_table,
	(uint32_t)&reset_handler,
};
