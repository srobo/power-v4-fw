#include <stdbool.h>

enum i2c_stat { I2C_STAT_NOTYET, I2C_STAT_DONE, I2C_STAT_ERR_AF,
		I2C_STAT_ERR_BERR};

void i2c_init();
void i2c_poll();
bool i2c_is_idle();
void i2c_init_read(uint8_t addr, uint8_t reg, volatile uint16_t *output,
		volatile enum i2c_stat *flag);
