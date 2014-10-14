#include <stdbool.h>

#include "battery.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "led.h"

void battery_init(void) {
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

	// We poll the battery current sense / voltage sense to see whether
	// something is wrong. Reads that are too close together cause the
	// INA219 to croak. Therefore set up a timer to periodically trigger
	// reads.
	// The IC settles readings at 2KHz; enable a timer @ 4Khz and read
	// voltage / current every other tick.
	rcc_periph_clock_enable(RCC_TIM2);
	timer_reset(TIM2);
	timer_set_prescaler(TIM2, 1799); // 72Mhz -> 40Khz
	timer_set_period(TIM2, 10); // 10 ticks -> 4Khz
	nvic_enable_irq(NVIC_TIM2_IRQ);
	nvic_set_priority(NVIC_TIM2_IRQ, 1);
	timer_enable_update_event(TIM2);
	timer_enable_irq(TIM2, TIM_DIER_UIE);
	timer_enable_counter(TIM2);
}

static uint32_t reg32 __attribute__((unused));
static uint32_t i2c = I2C1;

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
static bool i2c_error;     // Acknowledge failure or bus failure occurred.

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
		} else if (I2C_SR1(i2c) & I2C_SR1_AF) {
			// Acknowledge failure: the INA did not respond
			// Not a lot we can do about this, except count the
			// accumulated errors.
			I2C_SR1(i2c) &= ~I2C_SR1_AF;
			i2c_error = true;
			i2c_state = I2C_IDLE;
		}
		break;;
	case I2C_WRITE_DATA:
		// We're writing data; has it gone out and been ack'd?
		// Additionally await BTF, meaning it's been acknowledged(?)
		if (I2C_SR1(i2c) & (I2C_SR1_TxE | I2C_SR1_BTF)) {
			// Transmit done. Now stop and await the bus being free
			 i2c_send_stop(i2c);
			 i2c_state = I2C_WRITE_STOP;
		} else if (I2C_SR1(i2c) & I2C_SR1_AF) {
			// Ack failure again
			I2C_SR1(i2c) &= ~I2C_SR1_AF;
			i2c_error = true;
			i2c_state = I2C_IDLE;
		}
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
		} else if (I2C_SR1(i2c) & I2C_SR1_AF) {
			// Acknowledge failure: the INA did not respond
			I2C_SR1(i2c) &= ~I2C_SR1_AF;
			i2c_error = true;
			i2c_state = I2C_IDLE;
		}
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
		break;
	case I2C_READ_STOP:
		if (I2C_SR2(i2c) & I2C_SR2_BUSY)
			break;
		i2c_state = I2C_IDLE;
		break;
	}

	// Silence the compiler
	(void)u32;

	return;
}

static void set_reg_pointer( uint8_t a, uint8_t r )
{
	// EV5
	while (!((I2C_SR1(i2c) & I2C_SR1_SB)
	        & (I2C_SR2(i2c) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	i2c_send_7bit_address(i2c, a, I2C_WRITE);
	// /EV5


	// EV6
	while (!(I2C_SR1(i2c) & I2C_SR1_ADDR));
	reg32 = I2C_SR2(i2c);
	// /EV6

	// EV8_1
	i2c_send_data(i2c, r);
	// /EV8_1
	
	// EV8_2
	while (!(I2C_SR1(i2c) & ( I2C_SR1_BTF | I2C_SR1_TxE )));
	// /EV8_2
}

static uint16_t read_reg(uint8_t addr, uint8_t reg)
{

	i2c_send_start(i2c);

	set_reg_pointer(addr, reg);

	i2c_send_start(i2c);

	// EV5
	while (!((I2C_SR1(i2c) & I2C_SR1_SB)
	        & (I2C_SR2(i2c) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	// Set POS and ACK
	I2C_CR1(i2c) |= (I2C_CR1_POS | I2C_CR1_ACK);

	i2c_send_7bit_address(i2c, addr, I2C_READ);
	// /EV5

	// EV6
	while (!(I2C_SR1(i2c) & I2C_SR1_ADDR));
	reg32 = I2C_SR2(i2c);
	// /EV6

	// Clear ACK
	I2C_CR1(i2c) &= ~I2C_CR1_ACK;

	// EV7_3
	while (!(I2C_SR1(i2c) & I2C_SR1_BTF));
	i2c_send_stop(i2c);
	// /EV7_3
	
	uint16_t val;
	val = i2c_get_data(i2c) << 8;
	val |= i2c_get_data(i2c);

	// Clear POS
	I2C_CR1(i2c) &= ~I2C_CR1_POS;

	return val;
}

static uint16_t f_reg(uint8_t r)
{
	return read_reg(0x41, r);
}

uint16_t f_vshunt() { return f_reg(0x02); }
uint16_t f_vbus() { return f_reg(0x01); }

static uint16_t battery_reg(uint8_t r)
{
	return read_reg(0x40, r);
}

uint16_t battery_vshunt() { return battery_reg(0x01); }

uint16_t battery_vbus()
{
	uint16_t vbus = battery_reg(0x02);
	// Lower 3 bits are status bits. Rest is the voltage, measured in units
	// of 4mV. So, mask the lower 3 bits, then shift down by one.
	vbus &= 0xFFF8;
	vbus >>= 1;
	return vbus;
}

static uint32_t battery_current()
{
	uint16_t vshunt = battery_vshunt();

	// The measurement just taken is measured in 10uV units over the 500uO
	// resistor pair on the battery rail. I = V/R, and R being small,
	// multiply by 20 to give a figure measured in units of 1mA.
	uint32_t current = vshunt * 20;

	// Additionally, the current drawn is consistently reported as being
	// 800mA over reality; adjust this amount.
	if (current < 800) {
		// It's also super jittery, and might dip below this offset
		current = 1;
	} else {
		current -= 800;
	}

	return current;
}

volatile bool batt_do_read = false;
uint32_t batt_read_current = 0;
uint32_t batt_read_voltage = 0;
enum { BATT_READ_CURR, BATT_READ_VOLT } batt_read_state = BATT_READ_CURR;

void tim2_isr(void)
{
	batt_do_read = true;
	TIM_SR(TIM2) = 0; // Inexplicably does not reset
	return;
}

void battery_poll()
{
	bool do_read = false;

	nvic_disable_irq(NVIC_TIM2_IRQ);
	if (batt_do_read) {
		// First, reset the counter, so there's another 250uS til the
		// next read
		timer_set_counter(TIM2, 0);
		// Then reset the read-flag
		batt_do_read = false;

		do_read = true;
	}
	nvic_enable_irq(NVIC_TIM2_IRQ);

	if (!do_read)
		return;

	// We're clear to read.
	switch (batt_read_state) {
	case BATT_READ_CURR:
		batt_read_state = BATT_READ_VOLT;
		batt_read_current = battery_current();
		break;
	case BATT_READ_VOLT:
		batt_read_state = BATT_READ_CURR;
		batt_read_voltage = battery_vbus();
		break;
	}

	return;
}

// Upon read, return the most recently sampled result.
uint16_t read_battery_voltage()
{
	return batt_read_voltage;
}

uint32_t read_battery_current()
{
	return batt_read_current;
}
