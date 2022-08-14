#include "i2c.h"

// A timed out I2C device will NACK after receiving a byte
#define I2C_EXIT_ON_FAILED(x) if(i2c_timed_out) { return x;}
#define I2C_FAIL_ON_NACK(x) if(I2C_SR1(I2C1) & I2C_SR1_AF) { \
    i2c_timed_out = true; \
    if (I2C_SR2(I2C1) & I2C_SR2_BUSY) {i2c_send_stop(I2C1);} return x;}


volatile bool i2c_timed_out = false;
INA219_offset_t INA219_offsets[NUM_INA219] = {0};

void i2c_init(void){
    // Set I2C alternate functions on PB6 & PB7
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
              GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_I2C1_SCL);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
              GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO_I2C1_SDA);

    // Enable clock for I2C
    rcc_periph_clock_enable(RCC_I2C1);

    i2c_reset(I2C1);

    // Set APB1 to 14MHz
    i2c_set_clock_frequency(I2C1, 14);
    // Configure clocks for 400kHz I2C
    i2c_set_speed(I2C1, i2c_speed_fm_400k, 14);

    i2c_peripheral_enable(I2C1);
}

void i2c_start_message(uint8_t addr){
    uint32_t reg32 __attribute__((unused));

    I2C_EXIT_ON_FAILED();

    // Send START condition.
    i2c_send_start(I2C1);

    // Waiting for START to send and switch to master mode.
    while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
        && (I2C_SR2(I2C1) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

    // Say to what address we want to talk to.
    i2c_send_7bit_address(I2C1, addr, I2C_WRITE);

    // Waiting for address to transfer.
    while (!(I2C_SR1(I2C1) & (I2C_SR1_ADDR | I2C_SR1_AF)));
    I2C_FAIL_ON_NACK();

    // Cleaning ADDR condition sequence.
    reg32 = I2C_SR2(I2C1);
}

void i2c_stop_message(void){
    I2C_EXIT_ON_FAILED();

    // Wait for the data register to be empty or a NACK to be generated.
    while (!(I2C_SR1(I2C1) & (I2C_SR1_TxE | I2C_SR1_AF)));
    I2C_FAIL_ON_NACK();  /// TODO Is a NACK expected here?

    // Send STOP condition.
    i2c_send_stop(I2C1);
}

void i2c_send_byte(char c){
    I2C_EXIT_ON_FAILED();

    i2c_send_data(I2C1, c);
    // Wait for byte to complete transferring
    while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_AF)));
    I2C_FAIL_ON_NACK();
}

bool i2c_recv_bytes(uint8_t addr, uint8_t* buf, uint8_t len) {
    uint32_t reg32 __attribute__((unused));
    I2C_EXIT_ON_FAILED(false);

    if (len == 0) {
        return false;
    }

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
        I2C_FAIL_ON_NACK(false);

        // Clear ADDR
        reg32 = I2C_SR2(I2C1);

        // Program the STOP bit.

        // Read the data after the RxNE flag is set.
        while (!(I2C_SR1(I2C1) & (I2C_SR1_RxNE | I2C_SR1_AF)));
        I2C_FAIL_ON_NACK(false);

        buf[0] = i2c_get_data(I2C1);
    } else if (len == 2) {
        // Set POS and ACK
        i2c_enable_ack(I2C1);
        i2c_nack_next(I2C1);

        // Waiting for address to transfer.
        while (!(I2C_SR1(I2C1) & (I2C_SR1_ADDR | I2C_SR1_AF)));
        I2C_FAIL_ON_NACK(false);

        // Clear ADDR
        reg32 = I2C_SR2(I2C1);

        // Clear ACK
        i2c_disable_ack(I2C1);

        // Wait for BTF to be set
        while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_AF)));
        I2C_FAIL_ON_NACK(false);

        // Program STOP
        i2c_send_stop(I2C1);

        // Read DR twice
        buf[0] = i2c_get_data(I2C1);
        buf[1] = i2c_get_data(I2C1);

        // Wait for transaction to complete
        while (I2C_SR2(I2C1) & I2C_SR2_BUSY);
        // Reset NACK control
        i2c_nack_current(I2C1);
    } else {  /// TODO this locks up
        // Waiting for address to transfer.
        while (!(I2C_SR1(I2C1) & (I2C_SR1_ADDR | I2C_SR1_AF)));
        I2C_FAIL_ON_NACK(false);

        uint8_t rem;
        for (uint8_t i=0; i<len; i++) {
            rem = len - i;
            if (rem == 3) {
                // Wait for DataN-2 to be received (RxNE = 1)
                while (!(I2C_SR1(I2C1) & (I2C_SR1_RxNE | I2C_SR1_AF)));
                I2C_FAIL_ON_NACK(false);
                // Wait for DataN-1 to be received (BTF = 1)
                while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_AF)));
                I2C_FAIL_ON_NACK(false);

                // Now DataN-2 is in DR and DataN-1 is in the shift register
                // Clear ACK bit
                i2c_disable_ack(I2C1);

                // read byte from DR (DataN-2). This will launch the DataN reception in the shift register
                buf[i] = i2c_get_data(I2C1);
            } else if (rem == 2) {
                // Program STOP bit
                i2c_send_stop(I2C1);

                // Wait for the receive register to not be empty
                while (!(I2C_SR1(I2C1) & (I2C_SR1_RxNE | I2C_SR1_AF)));
                I2C_FAIL_ON_NACK(false);

                // read byte from DR (DataN-1)
                buf[i] = i2c_get_data(I2C1);
            } else if (rem == 1) {
                // Wait for the receive register to not be empty
                while (!(I2C_SR1(I2C1) & (I2C_SR1_RxNE | I2C_SR1_AF)));
                I2C_FAIL_ON_NACK(false);

                // read byte from DR (DataN)
                buf[i] = i2c_get_data(I2C1);
            } else {
                // Wait for the receive register to not be empty
                while (!(I2C_SR1(I2C1) & (I2C_SR1_RxNE | I2C_SR1_AF)));
                I2C_FAIL_ON_NACK(false);

                // read byte from DR
                buf[i] = i2c_get_data(I2C1);
            }
        }
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

    // store offset in first available index
    for (uint8_t i=0; i < NUM_INA219; i++) {
        if (INA219_offsets[i].addr == 0) {
            INA219_offsets[i].addr = addr;
            INA219_offsets[i].offset = current;
            return;
        }
    }
}

static int16_t get_current_offset_value(uint8_t addr) {
    // lookup offset with matching address
    for (uint8_t i=0; i < NUM_INA219; i++) {
        if (INA219_offsets[i].addr == addr) {
            return INA219_offsets[i].offset;
        }
    }

    // return 0 if no match found
    return 0;
}

void init_i2c_sensors(void) {
    init_current_sense(BATTERY_SENSE_ADDR, I_CAL_VAL(0.0005 * 10), INA219_CONF(0b00, 0b1100));  // Use 10mA LSB
    init_current_sense(REG_SENSE_ADDR, I_CAL_VAL(0.010), INA219_CONF(0b11, 0b0011));
}

void init_current_sense(uint8_t addr, uint16_t cal_val, uint16_t conf_val) {
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
    set_current_offset_value(addr);
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
