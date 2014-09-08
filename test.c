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

#define DELAY 2000

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

void flash(int led) {
	led_set(led);
	delay(DELAY);
	led_clear(led);
	delay(DELAY);
}

void out_on(void) {
	for (int j=0; j<6; j++) {
	//	delay(DELAY);
		output_on(j);
	}
}

void out_off(void) {
	for (int j=0; j<6; j++) {
		delay(DELAY);
		output_off(j);
	}
}

int main(void) {
	int k = 0;
	int j = 0;
	bool fan_state = false;

	init();

	delay(9999);
	/*usbd_dev = cdcacm_init();
	for (int x=0; x < 1000000; x++)
		cdcacm_poll(usbd_dev);

	smps_on();*/

	delay(10000);

	for (int x=0; x < 5000; x++) {
		piezo_toggle();
		delay(1);
	}
	delay(5000);
	for (int x=0; x < 5000; x++) {
		piezo_toggle();
		delay(1);
	}
	delay(5000);
	for (unsigned int x=0; x < 10000; x++) {
		piezo_toggle();
		delay(1);
	}

		for (int j=0; j<6; j++) {
			output_stat_on(j);
		}
	out_on();

	while(1);

	//fan_on();


//	out_on();

	//smps_on();
	//
	while (1) {
		cdcacm_poll(usbd_dev);
/*		flash(LED_RUN);
		flash(LED_ERROR);*/
		for (int j=0; j<6; j++) {
			output_stat_on(j);
		}
//		smps_on();
		
		//delay(DELAY);
		for (int j=0; j<6; j++) {
			output_stat_off(j);
		}
		//delay(DELAY);
		if (k == 200000) {
			if (fan_state) {
				output_off(4);
				output_on(0);
				smps_boost_off();
				fan_state = false;
			} else {
				output_on(4);
			//	output_off(0);
				smps_boost_on();
				fan_state = true;
			}
			led_toggle_flat();
			k = 0;
		}

		if ((k % 1000) == 0) {
			//char blah[20];
			uint16_t bv = battery_voltage();
			uint16_t bc = battery_current();
			uint16_t fv = f_v();
			uint16_t fi = f_i();
			bool p = pswitch_read();
			bool bi = button_int_read();
			bool be = button_ext_read();
			iprintf( "%06u  %06u  %06u  %06u\r\n", a, b, c, d);

			//fwrite( &v, 2, 1, stdout );
			//fflush( stdout );
			//iprintf( "%i %i %05d  %07d  %05d  %05d\r\n", bi, be, ((v >> 3) / 4)*16, (c -30)*180, ((fv>>3)/4)*16, fi );
			//cdcacm_send(usbd_dev, "beeeees\n");
			//cdcacm_send(usbd_dev, blah);
			led_toggle(LED_RUN);
			led_toggle(LED_ERROR);
/*			if (bi)
				cdcacm_send( usbd_dev, "face\n" );
			else
				cdcacm_send( usbd_dev, "bees\n" );
				*/
		}

		k++;
	}

	return 0;
}
