#include <stdbool.h>

enum i2c_stat { I2C_STAT_NOTYET, I2C_STAT_DONE, I2C_STAT_ERR };

void i2c_init();
bool i2c_is_idle();
void i2c_init_read(uint8_t addr, uint8_t reg, uint16_t *output,
		enum i2c_stat *flag);
