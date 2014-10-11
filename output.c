#include <stdbool.h>

#include "output.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

uint32_t OUTPUT_PORT[6] = {GPIOB,  GPIOB,  GPIOC, GPIOC, GPIOC, GPIOC};
uint16_t OUTPUT_PIN[6]  = {GPIO10, GPIO11, GPIO6, GPIO7, GPIO8, GPIO9};

uint32_t OUTPUT_STAT_PORT[6] = {GPIOC,  GPIOC,  GPIOB,  GPIOB,  GPIOB,  GPIOB};
uint16_t OUTPUT_STAT_PIN[6]  = {GPIO10, GPIO11, GPIO12, GPIO13, GPIO14, GPIO15};

uint32_t OUTPUT_CSDIS_PORT[4] = {GPIOC, GPIOC, GPIOC, GPIOC};
uint16_t OUTPUT_CSDIS_PIN[4] = {GPIO0, GPIO1, GPIO2, GPIO3};

// State of which IC we're reading current sense from
volatile enum { CSREAD_U5 = 0, CSREAD_U6 = 1, CSREAD_U7 = 2, CSREAD_U8 = 3,
	CSREAD_MAX = CSREAD_U8} csread_ic = CSREAD_U5;
volatile bool output_read_done = false;

// Current sense samples for each IC. Indexed by IC, then output.
uint32_t current_sense_samples[4][2];

void output_init(void) {
	for (int i = 0; i < 6; i++) {
		gpio_clear(OUTPUT_PORT[i], OUTPUT_PIN[i]);
		gpio_set_mode(OUTPUT_PORT[i], GPIO_MODE_OUTPUT_2_MHZ,
		              GPIO_CNF_OUTPUT_PUSHPULL, OUTPUT_PIN[i]);

		gpio_clear(OUTPUT_STAT_PORT[i], OUTPUT_STAT_PIN[i]);
		gpio_set_mode(OUTPUT_STAT_PORT[i], GPIO_MODE_OUTPUT_2_MHZ,
		              GPIO_CNF_OUTPUT_PUSHPULL, OUTPUT_STAT_PIN[i]);
	}

	// Configure current-sense disable pins, set initially to high
	// (disabled)
	for (int i = 0; i < 4; i++) {
		gpio_clear(OUTPUT_CSDIS_PORT[i], OUTPUT_CSDIS_PIN[i]);
		gpio_set_mode(OUTPUT_CSDIS_PORT[i], GPIO_MODE_OUTPUT_2_MHZ,
		              GPIO_CNF_OUTPUT_PUSHPULL, OUTPUT_CSDIS_PIN[i]);
		gpio_set(OUTPUT_CSDIS_PORT[i], OUTPUT_CSDIS_PIN[i]);
	}

	// Then enable the first IC.
	gpio_clear(OUTPUT_CSDIS_PORT[0], OUTPUT_CSDIS_PIN[0]);
}

void output_on(uint8_t n) {
	if (n > 6) return;
	gpio_set(OUTPUT_PORT[n], OUTPUT_PIN[n]);
}

void output_off(uint8_t n) {
	if (n > 6) return;
	gpio_clear(OUTPUT_PORT[n], OUTPUT_PIN[n]);
}

void output_stat_on(uint8_t n) {
	if (n > 6) return;
	gpio_set(OUTPUT_STAT_PORT[n], OUTPUT_STAT_PIN[n]);
}

void output_stat_off(uint8_t n) {
	if (n > 6) return;
	gpio_clear(OUTPUT_STAT_PORT[n], OUTPUT_STAT_PIN[n]);
}

// To be called from ADC intr
void current_sense_recvsamples(uint32_t samp1, uint32_t samp2)
{

	current_sense_samples[csread_ic][0] = samp1;
	current_sense_samples[csread_ic][1] = samp2;
	output_read_done = true;
}

void current_sense_poll()
{
	if (output_read_done) {
		// Disable current sense output from the current IC
		gpio_set(OUTPUT_CSDIS_PORT[csread_ic], OUTPUT_CSDIS_PIN[csread_ic]);

		// Switch to the next IC. IRQ reads this, so disable for update.
		nvic_disable_irq(NVIC_ADC1_2_IRQ);
		output_read_done = false;
		csread_ic++;
		if (csread_ic > CSREAD_MAX)
			csread_ic = 0;
		nvic_enable_irq(NVIC_ADC1_2_IRQ);

		// Enable current sense
		gpio_clear(OUTPUT_CSDIS_PORT[csread_ic], OUTPUT_CSDIS_PIN[csread_ic]);
	}
}
