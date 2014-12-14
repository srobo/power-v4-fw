#include <stdbool.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/cm3/nvic.h>

#include "i2c.h"

// I2C state progression: it may seem that there are distinct write and
// read cycles in this state machine, but there aren't: we only ever write one
// byte at an INA219 (the reg on it to read) then read that register from it.
// Therefore we start in I2C_WRITE_START, cycle all the way through to
// I2C_READ_STOP, and then transition to idle.
enum {
	I2C_IDLE,
	I2C_WRITE_START, I2C_WRITE_ADDR, I2C_WRITE_DATA, I2C_WRITE_STOP,
	I2C_READ_START, I2C_READ_ADDR, I2C_READ_DATA, I2C_READ_STOP
} i2c_state = I2C_IDLE;

// Data for performing a transaction with the INA219.
static uint8_t ina_addr;   // Address on the bus. 0x40 for batt, 0x41 for smps
static uint8_t ina_reg;    // Which INA reg to read. 1 = vshunt, 2 = vbus
static uint16_t ina_result;// Read value is read here.

// Target pointer and flag to set when request has finished.
static volatile uint16_t *output_ptr;
static volatile enum i2c_stat *output_done_ptr;

static uint32_t i2c = I2C1;

void i2c_init()
{
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
                      GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO6 | GPIO7);
        rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_I2C1EN);
        rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_AFIOEN);

        i2c_reset(I2C1);
        i2c_set_clock_frequency(I2C1, I2C_CR2_FREQ_36MHZ);
        i2c_set_fast_mode(I2C1);
        i2c_set_ccr(I2C1, 30);
        i2c_set_trise(I2C1, 0x0b);
        i2c_peripheral_enable(I2C1);

	// Enable buffer-free/full, event, and error intrs.
	I2C_CR2(i2c) = I2C_CR2(i2c) | I2C_CR2_ITBUFEN | I2C_CR2_ITEVTEN
			| I2C_CR2_ITERREN;
}

static ack_fail_signalled = false;
static berr_fail_signalled = false;

static void check_ack_fail()
{
	if (I2C_SR1(i2c) & I2C_SR1_AF) {
		// Acknowledge failure: the INA did not respond
		// Not a lot we can do about this, except count the
		// accumulated errors.
		I2C_SR1(i2c) &= ~I2C_SR1_AF;
		*output_done_ptr = I2C_STAT_ERR_AF;
		i2c_state = I2C_IDLE;
		ack_fail_signalled = true;
	}
	return;
}

static void check_berr()
{
	if (I2C_SR1(i2c) & I2C_SR1_BERR) {
		// Bus error -- we did something wrong. Oh dear.
		I2C_SR1(i2c) &= ~I2C_SR1_BERR;
		*output_done_ptr = I2C_STAT_ERR_BERR;
		i2c_state = I2C_IDLE;
		berr_fail_signalled = true;
	}
	return;
}

// To be called with i2c intrs disabled
void i2c_fsm(void)
{
	volatile uint32_t u32;

	switch (i2c_state) {
	case I2C_IDLE:
		// Activity is initiated by some other function call
		break;
	case I2C_WRITE_START:
		// A start has been initiated; has a start condition occurred?
		if (I2C_SR1(i2c) & I2C_SR1_SB) {
			// It will be cleared when we load the data register.
			// In the meantime, write an address.
			i2c_send_7bit_address(i2c, ina_addr, I2C_WRITE);
			i2c_state = I2C_WRITE_ADDR;
		}
		break;
	case I2C_WRITE_ADDR:
		// An address has been written; did we get an ack?
		if (I2C_SR1(i2c) & I2C_SR1_ADDR) {
			// Clear by reading SR2
			u32 = I2C_SR2(i2c);
			// Send data
			i2c_send_data(i2c, ina_reg);
			i2c_state = I2C_WRITE_DATA;
		}
		check_ack_fail();
		check_berr();
		break;;
	case I2C_WRITE_DATA:
		// We're writing data; has it gone out and been ack'd?
		// Additionally await BTF, meaning it's been acknowledged(?)
		if (I2C_SR1(i2c) & (I2C_SR1_TxE | I2C_SR1_BTF)) {
			// Transmit done. Now stop and await the bus being free
			 i2c_send_stop(i2c);
			 i2c_state = I2C_WRITE_STOP;
		}
		check_ack_fail();
		check_berr();
		break;
	case I2C_WRITE_STOP:
		// Await the bus being free.
		if (I2C_SR2(i2c) & I2C_SR2_BUSY)
			break;

		// Yes, this is probably pointless, but essentially costless
		__asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");
		__asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");
		__asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");

		// Start condition for reading data.
		i2c_send_start(i2c);
		// We're now reading
		i2c_state = I2C_READ_START;
		break;
	case I2C_READ_START:
		// A start has been initiated; has a start condition occurred?
		if (I2C_SR1(i2c) & I2C_SR1_SB) {
			// Write an address.
			i2c_send_7bit_address(i2c, ina_addr, I2C_READ);
			// Set acknowledge bit, to acknowledge the first
			// byte that turns up on the wire.
			i2c_enable_ack(i2c);
			// Set a magical flag that makes the ack bit apply to
			// the /next/ byte after this. See below.
			i2c_nack_next(i2c);
			i2c_state = I2C_READ_ADDR;
		}
		break;
	case I2C_READ_ADDR:
		// An address has been written; did we get an ack?
		if (I2C_SR1(i2c) & I2C_SR1_ADDR) {
			// Clear address bit by reading SR2
			u32 = I2C_SR2(i2c);
			// We're now to await bytes being sent by the INA.
			i2c_state = I2C_READ_DATA;
			// Disable ack -- due to the POS bit being set (in the
			// call to i2c_nack_next above), the next byte to be
			// read will be ack'd, and the next nack'd, allowing 2
			// bytes to be read. I would describe this special dance
			// as being "an off by one in hardware".
			i2c_disable_ack(i2c);

		}
		check_ack_fail();
		check_berr();
		break;
	case I2C_READ_DATA:
		// Await BTF flag being set. This means that a) there is a byte
		// read, currently stored in the data register, b) another byte
		// has been read and is stuck in the shift register, and c) we
		// are currently stretching the clock. Due to the ack/nack dance
		// above, at this point the two bytes received have been ack'd
		// and nack'd respectively.
		if (I2C_SR1(i2c) & I2C_SR1_BTF) {
			// Put a stop bit on the bus; no more data.
			i2c_send_stop(i2c);
			// Now read data from the data register. Once for the
			// first byte, then a second is loaded into DR after
			// the read.
			ina_result = i2c_get_data(i2c);
			ina_result <<= 8;
			ina_result |= i2c_get_data(i2c);

			// Enter state waiting for the stop bit to have appeared
			// on the bus
			i2c_state = I2C_READ_STOP;
		}
		check_ack_fail();
		check_berr();
		break;
	case I2C_READ_STOP:
		if (I2C_SR2(i2c) & I2C_SR2_BUSY)
			break;

		// Reset POS bit after the reading dance above.
		i2c_nack_current(i2c);

		// Set output data fields
		*output_ptr = ina_result;
		*output_done_ptr = I2C_STAT_DONE;
		i2c_state = I2C_IDLE;
		break;
	}

	// Silence the compiler
	(void)u32;

	return;
}

void i2c_poll()
{
	i2c_fsm();
}

bool i2c_is_idle()
{
	return i2c_state == I2C_IDLE;
}

void i2c_init_read(uint8_t addr, uint8_t reg, volatile uint16_t *output,
			volatile enum i2c_stat *flag)
{
	ina_addr = addr;
	ina_reg = reg;
	ina_result = 0;
	output_ptr = output;
	output_done_ptr = flag;
	*output = 0;
	*flag = I2C_STAT_NOTYET;

	// Initiate start condition
	i2c_state = I2C_WRITE_START;
	i2c_send_start(i2c);
}
