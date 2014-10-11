#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>

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

#define DELAY 4000

#define delay(x) do { for (int i = 0; i < x * 1000; i++) \
                          __asm__("nop"); \
                    } while(0)

usbd_device *usbd_dev;

void init(void) {
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPAEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPBEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPCEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPDEN);
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
}

void flash_output(int i) { output_on(i); delay(DELAY); output_off(i); }
void flash_stat(int i) { output_stat_on(i); delay(DELAY); output_stat_off(i); }

int main(void) {
	init();

	while(1) {
		// Outputs
		flash_output(0);
		flash_output(2);
		flash_output(3);
		flash_output(4);
		flash_output(5);
		flash_output(1);
		delay(DELAY);

		// Output status LEDs
		flash_stat(0);
		flash_stat(2);
		flash_stat(3);
		flash_stat(4);
		flash_stat(5);
		flash_stat(1);
		delay(DELAY);

		// Fan
		fan_on();
		delay(4*DELAY);
		fan_off();
		delay(DELAY);

		// 5V SMPS
		smps_on();
		delay(DELAY);
		smps_off();
		delay(DELAY);

		// Piezo
		for (int i=0; i<2000; i++) {
			piezo_toggle();
			delay(1);
		}

		// I2C U1
		battery_vbus();

		// Run LED (also indicates if I2C works)
		led_set(LED_RUN);
		delay(DELAY);
		led_clear(LED_RUN);
		delay(DELAY);

		// I2C U4
		f_vbus();

		// Error LED
		led_set(LED_ERROR);
		delay(DELAY);
		led_clear(LED_ERROR);
		delay(DELAY);
	}

	return 0;
}
