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
#include "usb.h"

void
init()
{

	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPAEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPBEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPCEN);
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPDEN);

	usb_init();
}

int
main()
{

	init();

	while (1) {
		usb_poll();
		// Do things
	}
}
