#include <stdint.h>
#include <string.h>
#include "piezo.h"
#include <libopencm3/stm32/gpio.h>

#define PIEZO_PORT GPIOB
#define PIEZO_PIN GPIO0

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef struct {
	uint16_t freq;
	uint16_t duration;
} piezo_sample_t;

/* Ringbuffer of piezo samples */
static piezo_sample_t sample_buffer[PIEZO_BUFFER_LEN];
static unsigned int buffer_free_pos = 0;
static unsigned int buffer_cur_pos = 0;

void piezo_init(void) {
	gpio_clear(PIEZO_PORT, PIEZO_PIN);
	gpio_set_mode(PIEZO_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, PIEZO_PIN);
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
	}

	return false;
}
