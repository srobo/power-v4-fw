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
	I2C_READ_START, I2C_READ_ADDR, I2C_READ_DATA1, I2C_READ_DATA2,
	I2C_READ_STOP
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

	nvic_enable_irq(NVIC_I2C1_EV_IRQ);
	nvic_set_priority(NVIC_I2C1_EV_IRQ, 1);
	// Enable buffer-free/full, event, and error intrs.
	I2C_CR2(i2c) = I2C_CR2(i2c) | I2C_CR2_ITBUFEN | I2C_CR2_ITEVTEN
			| I2C_CR2_ITERREN;
}

static void check_ack_fail()
{
	if (I2C_SR1(i2c) & I2C_SR1_AF) {
		// Acknowledge failure: the INA did not respond
		// Not a lot we can do about this, except count the
		// accumulated errors.
		I2C_SR1(i2c) &= ~I2C_SR1_AF;
		*output_done_ptr = I2C_STAT_ERR_AF;
		i2c_state = I2C_IDLE;
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

		// No longer busy. However we need to wait 160ns before
		// starting again or the INA croaks. @72Mhz, that's 12 cycles.
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
			i2c_send_7bit_address(i2c, ina_addr, I2C_WRITE);
			// Set acknowledge bit, to acknowledge the first
			// byte that turns up on the wire.
			i2c_enable_ack(i2c);
			i2c_state = I2C_READ_ADDR;
		}
		break;
	case I2C_READ_ADDR:
		// An address has been written; did we get an ack?
		if (I2C_SR1(i2c) & I2C_SR1_ADDR) {
			// Clear by reading SR2
			u32 = I2C_SR2(i2c);
			// We're now to await first byte being sent by the INA.
			i2c_state = I2C_READ_DATA1;
		}
		check_ack_fail();
		check_berr();
		break;
	case I2C_READ_DATA1:
		// We're awaiting a byte turning up.
		if (I2C_SR1(i2c) & I2C_SR1_RxNE) {
			// Excellent, we have a byte.
			ina_result = i2c_get_data(i2c);
			// We now need to clear the ack bit and send a stop
			// condition, so that after the next byte arriving,
			// it's promptly nack'd and stopped.
			i2c_disable_ack(i2c);
			i2c_send_stop(i2c);
			i2c_state = I2C_READ_DATA2;
		}
		check_ack_fail();
		check_berr();
		break;
	case I2C_READ_DATA2:
		if (I2C_SR1(i2c) & I2C_SR1_RxNE) {
			// Excellent, second byte
			ina_result <<= 8;
			ina_result |= i2c_get_data(i2c);
			// Stop will be sent automatically; don't go to idle
			// until the bus is free though.
			i2c_state = I2C_READ_STOP;
		}
		check_ack_fail();
		check_berr();
		break;
	case I2C_READ_STOP:
		if (I2C_SR2(i2c) & I2C_SR2_BUSY)
			break;
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

void i2c1_ev_isr(void)
{
	i2c_fsm();
	return;
}

void i2c_poll()
{
	nvic_disable_irq(NVIC_I2C1_EV_IRQ);
	i2c_fsm();
	nvic_enable_irq(NVIC_I2C1_EV_IRQ);
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
	nvic_disable_irq(NVIC_I2C1_EV_IRQ);
	i2c_state = I2C_WRITE_START;
	nvic_enable_irq(NVIC_I2C1_EV_IRQ);
	i2c_send_start(i2c);
}
