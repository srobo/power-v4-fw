#include "i2c.h"
#include "global_vars.h"

// AF bit is set when a byte transfer ends with a NACK
static inline bool nack_recieved(void) {return I2C_SR1(I2C1) & I2C_SR1_AF;}
static inline bool i2c_transaction_in_progress(void) {return I2C_SR2(I2C1) & I2C_SR2_BUSY;}

// A timed out I2C device will NACK after receiving a byte
#define I2C_RETURN_IF_FAILED(x) if(i2c_timed_out) { return x;}
// The I2C protocol means that every transfer will end with either a NACK or ACK
// The ACK requires the slave device to be actively participating in the transaction
#define I2C_FAIL_AND_RETURN_ON_NACK(x) if(nack_recieved()) { \
    i2c_timed_out = true; \
    if (i2c_transaction_in_progress()) {i2c_send_stop(I2C1);} return x;}


volatile bool i2c_timed_out = false;
// this can be 2 to cover the connected INA219's but 16 covers all of the configurable addresses
int16_t ina219_offsets[16] = {0};

void i2c_init(void) {
    // Set I2C alternate functions on PB6 & PB7
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
              GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_I2C1_SCL);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
              GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO_I2C1_SDA);

    // Enable clock for I2C
    rcc_periph_clock_enable(RCC_I2C1);

    i2c_reset(I2C1);

    // Configure clocks for 400kHz I2C & 36MHz PCLK1 clock
    i2c_set_speed(I2C1, i2c_speed_fm_400k, 36);

    i2c_peripheral_enable(I2C1);
}

void i2c_start_message(uint8_t addr) {
    uint32_t reg32 __attribute__((unused));

    I2C_RETURN_IF_FAILED();

    // Send START condition.
    i2c_send_start(I2C1);

    // Waiting for START to send and switch to master mode.
    while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
        && (I2C_SR2(I2C1) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

    // Say to what address we want to talk to.
    i2c_send_7bit_address(I2C1, addr, I2C_WRITE);

    // Waiting for address to transfer.
    while (!(I2C_SR1(I2C1) & (I2C_SR1_ADDR | I2C_SR1_AF)));
    I2C_FAIL_AND_RETURN_ON_NACK();

    // Cleaning ADDR condition sequence.
    reg32 = I2C_SR2(I2C1);
}

void i2c_stop_message(void) {
    I2C_RETURN_IF_FAILED();

    // Wait for the data register to be empty or a NACK to be generated.
    while (!(I2C_SR1(I2C1) & (I2C_SR1_TxE | I2C_SR1_AF)));
    I2C_FAIL_AND_RETURN_ON_NACK();  /// TODO Is a NACK expected here?

    // Send STOP condition.
    i2c_send_stop(I2C1);
}

void i2c_send_byte(char c) {
    I2C_RETURN_IF_FAILED();

    i2c_send_data(I2C1, c);
    // Wait for byte to complete transferring
    while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_AF)));
    I2C_FAIL_AND_RETURN_ON_NACK();
}

bool i2c_recv_bytes(uint8_t addr, uint8_t* buf, uint8_t len) {
    uint32_t reg32 __attribute__((unused));
    I2C_RETURN_IF_FAILED(false);

    if (len == 0) {
        return false;
    }

    // Reset NACK control
    i2c_nack_current(I2C1);

    // Send START condition.
    i2c_send_start(I2C1);

    // Waiting for START to send and switch to master mode.
    while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
        && (I2C_SR2(I2C1) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

    // Say to what address we want to read from.
    i2c_send_7bit_address(I2C1, addr, I2C_READ);

    if (len == 1) {
        // clear the ACK bit.
        i2c_disable_ack(I2C1);

        // Waiting for address to transfer.
        while (!(I2C_SR1(I2C1) & (I2C_SR1_ADDR | I2C_SR1_AF)));
        I2C_FAIL_AND_RETURN_ON_NACK(false);

        // Clear ADDR
        reg32 = I2C_SR2(I2C1);

        // Program the STOP bit.
        i2c_send_stop(I2C1);

        // Read the data after the RxNE flag is set.
        while (!(I2C_SR1(I2C1) & I2C_SR1_RxNE));

        buf[0] = i2c_get_data(I2C1);
    } else if (len == 2) {
        // Set POS and ACK
        i2c_enable_ack(I2C1);
        i2c_nack_next(I2C1);

        // Waiting for address to transfer.
        while (!(I2C_SR1(I2C1) & (I2C_SR1_ADDR | I2C_SR1_AF)));
        I2C_FAIL_AND_RETURN_ON_NACK(false);

        // Clear ADDR
        reg32 = I2C_SR2(I2C1);

        // Clear ACK
        i2c_disable_ack(I2C1);

        // Wait for BTF to be set
        while (!(I2C_SR1(I2C1) & I2C_SR1_BTF));

        // Program STOP
        i2c_send_stop(I2C1);

        // Read DR twice
        buf[0] = i2c_get_data(I2C1);
        buf[1] = i2c_get_data(I2C1);

        // Wait for transaction to complete
        while (I2C_SR2(I2C1) & I2C_SR2_BUSY);
        // Reset NACK control
        i2c_nack_current(I2C1);
    } else {
        i2c_enable_ack(I2C1);

        // Waiting for address to transfer.
        while (!(I2C_SR1(I2C1) & (I2C_SR1_ADDR | I2C_SR1_AF)));
        I2C_FAIL_AND_RETURN_ON_NACK(false);

        // Clear ADDR
        reg32 = I2C_SR2(I2C1);

        uint8_t idx;
        for (idx = 0; idx < (len - 3); idx++) {
            // Read the data after the RxNE flag is set.
            while (!(I2C_SR1(I2C1) & I2C_SR1_RxNE));

            // Reading the data register clears RxNE
            buf[idx] = i2c_get_data(I2C1);
        }

        // DataN-2
        // Wait for DataN-2 to be received (RxNE = 1)
        while (!(I2C_SR1(I2C1) & I2C_SR1_RxNE));
        // Wait for DataN-1 to be received (BTF = 1)
        while (!(I2C_SR1(I2C1) & I2C_SR1_BTF));

        // Now DataN-2 is in DR and DataN-1 is in the shift register
        // Clear ACK bit
        i2c_disable_ack(I2C1);

        // read byte from DR (DataN-2). This will launch the DataN reception in the shift register
        buf[idx++] = i2c_get_data(I2C1);

        // DataN-1
        // Wait for DataN to be received (BTF = 1)
        while (!(I2C_SR1(I2C1) & I2C_SR1_BTF));

        // Program STOP bit
        i2c_send_stop(I2C1);

        // read byte from DR (DataN-1)
        buf[idx++] = i2c_get_data(I2C1);

        // DataN
        // Wait for the receive register to not be empty
        while (!(I2C_SR1(I2C1) & I2C_SR1_RxNE));

        // read byte from DR (DataN)
        buf[idx++] = i2c_get_data(I2C1);
    }
    return true;
}

static void set_current_offset_value(uint8_t addr) {
    // Set register pointer to current register
    i2c_start_message(addr);
    i2c_send_byte(0x04);
    i2c_stop_message();

    uint8_t val[2];
    bool i_success = i2c_recv_bytes(addr, val, 2);

    if (!i_success) {
        return;
    }

    int16_t current = (int16_t)(((uint16_t)val[0] << 8) | ((uint16_t)val[1] & 0xff));

    ina219_offsets[addr & 0xf] = current;
}

static int16_t get_current_offset_value(uint8_t addr) {
    // lookup offset with matching address
    return ina219_offsets[addr & 0xf];
}

void init_i2c_sensors(bool calc_offset) {
    init_current_sense(BATTERY_SENSE_ADDR, I_CAL_VAL(0.0005 * 10), INA219_CONF(0b00, 0b1100), calc_offset);  // Use 10mA LSB
    init_current_sense(REG_SENSE_ADDR, I_CAL_VAL(0.010), INA219_CONF(0b11, 0b1100), calc_offset);
}

void init_current_sense(uint8_t addr, uint16_t cal_val, uint16_t conf_val, bool calc_offset) {
    // Program calibration reg (0x05) w/ shunt value
    i2c_start_message(addr);
    i2c_send_byte(0x05);  // calibration reg address
    i2c_send_byte((uint8_t)((cal_val >> 8) & 0xff));
    i2c_send_byte((uint8_t)(cal_val & 0xff));
    i2c_start_message(addr);
    i2c_send_byte(0x00);  // configuration reg address
    i2c_send_byte((uint8_t)((conf_val >> 8) & 0xff));
    i2c_send_byte((uint8_t)(conf_val & 0xff));
    i2c_stop_message();
    if (calc_offset) {
        // wait for a measurement to be made
        delay(10);
        set_current_offset_value(addr);
    }
}

INA219_meas_t measure_current_sense(uint8_t addr) {
    // Set register pointer to current register
    i2c_start_message(addr);
    i2c_send_byte(0x04);
    i2c_stop_message();

    INA219_meas_t res = {0};

    uint8_t val[2];
    bool i_success = i2c_recv_bytes(addr, val, 2);

    res.current = (int16_t)(((uint16_t)val[0] << 8) | ((uint16_t)val[1] & 0xff));
    res.current -= get_current_offset_value(addr);

    // Set register pointer to voltage register
    i2c_start_message(addr);
    i2c_send_byte(0x02);
    i2c_stop_message();

    bool v_success = i2c_recv_bytes(addr, val, 2);
    res.voltage = (int16_t)(((uint16_t)val[0] << 8) | ((uint16_t)val[1] & 0xff));
    res.voltage &= 0xfff8;  // mask status bits
    res.voltage >>= 1;  // rshift to get 1mV/bit

    // did i2c timeout during the transaction?
    res.success = (!i2c_timed_out && i_success && v_success);

    return res;
}

void reset_i2c_watchdog(void) {
    // clear flag
    i2c_timed_out = false;

    // Disable and re-enable I2C to clear status bits
    I2C_CR1(I2C1) &= ~I2C_CR1_PE;
    I2C_CR1(I2C1) |= I2C_CR1_PE;
}
