#include <stdbool.h>

void i2c_init();
bool i2c_is_idle();
void i2c_init_read(uint8_t addr, uint8_t reg, uint16_t *output, bool *flag);
