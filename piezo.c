#include <stdint.h>
#include <string.h>
#include "piezo.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#define PIEZO_PORT GPIOB
#define PIEZO_PIN GPIO0

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* Inspired by Rich's piezo code from the v3 power board */

typedef struct {
	uint16_t freq;
	uint16_t duration;
} piezo_sample_t;

/* Ringbuffer of piezo samples */
static piezo_sample_t sample_buffer[PIEZO_BUFFER_LEN];
static unsigned int buffer_free_pos = 0;
static unsigned int buffer_cur_pos = 0;

static unsigned int elapsed_piezo_time = 0;
static unsigned int piezo_duration = 0;

void piezo_init(void) {
	gpio_clear(PIEZO_PORT, PIEZO_PIN);
	gpio_set_mode(PIEZO_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, PIEZO_PIN);

	/* Enable TIM3 */
	rcc_periph_clock_enable(RCC_TIM3);
	timer_reset(TIM3);
	timer_set_prescaler(TIM3, 72); // 72Mhz -> 1Mhz
	timer_set_period(TIM3, 1); // 1Mhz, really configured elsewhere.
	nvic_set_priority(NVIC_TIM3_IRQ, 2); // Less important
	nvic_disable_irq(NVIC_TIM3_IRQ);
	timer_enable_update_event(TIM3);
	timer_enable_irq(TIM3, TIM_DIER_UIE);
	timer_enable_counter(TIM3);
}

void piezo_toggle(void) {
	gpio_toggle(PIEZO_PORT, PIEZO_PIN);
}

/* Calculate the number of free samples in the ringbuffer */
static unsigned int free_samples() {
	if (buffer_free_pos == buffer_cur_pos) {
		return PIEZO_BUFFER_LEN;
	} else if (buffer_free_pos < buffer_cur_pos) {
		return buffer_cur_pos - buffer_free_pos;
	} else {
		unsigned int tmp = buffer_free_pos - buffer_cur_pos;
		return PIEZO_BUFFER_LEN - tmp;
	}
}

static bool more_samples_available() {
	if (buffer_free_pos != buffer_cur_pos)
		return true;
}

static void configure_piezo_timer(piezo_sample_t *ps) {
	if (ps->freq == 0) {
		/* Zero freq -> be silent. Simply don't toggle the piezo */
		nvic_disable_irq(NVIC_TIM3_IRQ);
	} else {
		/* Calculate delay, in Mhz. To avoid massively thrashing intrs
		 * as a direct result of student written code, limit frequency
		 * to 10Khz. */
		unsigned int freq = MIN(10000, ps->freq);
		unsigned int delay = 1000000 / freq;
		/* Toggle at twice that speed. */
		delay *= 2;

		timer_set_period(TIM3, delay);
		TIM_SR(TIM3) = 0;
		nvic_enable_irq(NVIC_TIM3_IRQ);
	}
}

void
tim3_isr()
{
	piezo_toggle();
	TIM_SR(TIM3) = 0;
}

bool piezo_recv(uint32_t size, uint8_t *data) {
	if (size == 0)
		/* Nope */
		return true;

	if ((size & 3) != 0) {
		/* Not a multiple of 4 bytes. */
		return true;
	}

	unsigned int incoming_samples = size / 4;
	if (incoming_samples >= free_samples())
		/* Not enough space in the ringbuffer, including one free
		 * element in the middle */
		return true;

	/* There's enough space. Memcpy in. */
	/* XXX intrs */
	if (buffer_free_pos > buffer_cur_pos) {
		/* The buffer:
		 *
		 * [******XXXXXXXXX******]
		 *        |        |
		 *        cur      free
		 *
		 * Copy samples to the back of the buffer. */
		unsigned int samples_at_back =
			PIEZO_BUFFER_LEN - buffer_free_pos;
		unsigned int samples_to_copy =
			MIN(samples_at_back, incoming_samples);
		
		unsigned int bytes = samples_to_copy * 4;
		memcpy(&sample_buffer[buffer_free_pos], data, bytes);
		data += bytes;
		incoming_samples -= samples_to_copy;
		buffer_free_pos += samples_to_copy;
		buffer_free_pos %= PIEZO_BUFFER_LEN;
	}

	if (incoming_samples > 0) {
		/* If there are still samples to copy, the buffer is guarenteed
		 * to look like this:
		 *
		 * [XXXXXX*********XXXXXX]
		 *        |        |
		 *        free     cur
		 *
		 * Copy samples to the front/middle of the buffer. */
		unsigned int bytes = incoming_samples * 4;
		memcpy(&sample_buffer[buffer_free_pos], data, bytes);
		buffer_free_pos += incoming_samples;
		buffer_free_pos %= PIEZO_BUFFER_LEN;
	}

	return false;
}

void piezo_tick(void) {
	/* Update piezo -- this function is called at 1Khz. Only change what
	 * the piezo is doing if the current sample has completed it's duration
	 * or there is no current sample, both signified by the inverse of the
	 * next condition */
	if (elapsed_piezo_time != piezo_duration) {
		/* Currently sounding something. Increase tick and carry on. */
		elapsed_piezo_time++;
		return;
	}

	/* If there are no more samples, simply stop */
	if (buffer_free_pos == buffer_cur_pos) {
		nvic_disable_irq(NVIC_TIM3_IRQ);
		elapsed_piezo_time = 0;
		piezo_duration = 0;
		return;
	}

	/*  Otherwise, there must be one more sample to read. */
	configure_piezo_timer(&sample_buffer[buffer_cur_pos]);
	piezo_duration = sample_buffer[buffer_cur_pos].duration;
	elapsed_piezo_time = 0;
	buffer_cur_pos++;
	buffer_cur_pos %= PIEZO_BUFFER_LEN;
}
