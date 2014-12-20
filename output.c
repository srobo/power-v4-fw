#include <stdbool.h>

#include "output.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#define OUTPUT_CURRENT_IIR_DECAY 32
#define OUTPUT_CURRENT_IIR_LOG 5
#define HIGH_OUTPUT_CURLIMIT 20000 * OUTPUT_CURRENT_IIR_DECAY /* 20A */
#define LOW_OUTPUT_CURLIMIT 10000 * OUTPUT_CURRENT_IIR_DECAY /* 10A */

uint32_t current_limit_iirs[6];
static bool output_status[6];

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

	// Do not turn on if current limit tripped
	if (output_status[n])
		return;

	gpio_set(OUTPUT_PORT[n], OUTPUT_PIN[n]);
}

void output_off(uint8_t n) {
	if (n > 6) return;
	gpio_clear(OUTPUT_PORT[n], OUTPUT_PIN[n]);
}

void output_stat_on(uint8_t n) {
	if (n > 6) return;
	// Set signal led; also set current limit on
	gpio_set(OUTPUT_STAT_PORT[n], OUTPUT_STAT_PIN[n]);
	output_status[n] = true;
}

void output_stat_off(uint8_t n) {
	if (n > 6) return;
	// Clear LED and current limit status
	gpio_clear(OUTPUT_STAT_PORT[n], OUTPUT_STAT_PIN[n]);
	output_status[n] = false;
}

// To be called from ADC intr
void current_sense_recvsamples(uint32_t samp1, uint32_t samp2)
{

	current_sense_samples[csread_ic][0] = samp1;
	current_sense_samples[csread_ic][1] = samp2;
	output_read_done = true;
}

// XXX -- there's the risk that we'll update the target output IC just before
// the ADC interrupt fires, storing the wrong sample into the wrong place. This
// should be addressed later.
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

uint32_t current_sense_read(int output)
{
	uint32_t result = 0;
	// Don't allow ADC intr to fire when we're reading
	nvic_disable_irq(NVIC_ADC1_2_IRQ);
	switch (output) {
	case 0:
		result = current_sense_samples[0][0] +
			current_sense_samples[0][1];
		break;
	case 1:
		result = current_sense_samples[1][0] +
			current_sense_samples[1][1];
		break;
	case 2:
		result = current_sense_samples[2][0];
		break;
	case 3:
		result = current_sense_samples[2][1];
		break;
	case 4:
		result = current_sense_samples[3][0];
		break;
	case 5:
		result = current_sense_samples[3][1];
		break;
	}
	nvic_enable_irq(NVIC_ADC1_2_IRQ);

	// Convert this reading into milliamps. The specific ratio of output
	// current to current-sense-current is device specific, and
	// unfortunately appears to be output or IC specific on the power
	// boards. XXX, test with more than one board.
	// I (jmorse) have made some current measurements, and the ratio
	// doesn't appear to be linear either. As supplied current increases,
	// the ratio drops. Right now I can't test at high  currents due to
	// lack of supply, but when drawing 3A, each ADC lsb represents about
	// 7.5mA.
	// This is a reasonable approximation to work with. If the ratio
	// continues to drop at higher currents, then it's an overapproximation,
	// which is safe.
	// So, multiply the output by 7.5. Because this is a uC, use a cooked
	// approach.

	uint32_t tmp_result = 7*result;
	result >>=1;
	tmp_result += result;
	result = tmp_result;
	return result;
}

void
do_output_curlimit(unsigned int idx, unsigned int iir_limit_val)
{
	uint32_t sample = current_sense_read(idx);
        uint32_t decay = current_limit_iirs[idx] >> OUTPUT_CURRENT_IIR_LOG;
        current_limit_iirs[idx] -= decay;
        current_limit_iirs[idx] += sample;;

	// Check whether the IIR is over the specified current limit.
        if (current_limit_iirs[idx] > iir_limit_val) {
		// Turn off this output (permanently) and signal to the user.
		output_off(idx);
		output_stat_on(idx);
	}
}

void
output_poll(void)
{
	// To be called at 1Khz; mantains IIRs on each power board output,
	// enforcing a software current limit.
	do_output_curlimit(0, HIGH_OUTPUT_CURLIMIT);
	do_output_curlimit(1, HIGH_OUTPUT_CURLIMIT);
	do_output_curlimit(2, LOW_OUTPUT_CURLIMIT);
	do_output_curlimit(3, LOW_OUTPUT_CURLIMIT);
	do_output_curlimit(4, LOW_OUTPUT_CURLIMIT);
	do_output_curlimit(5, LOW_OUTPUT_CURLIMIT);
	return;
}
