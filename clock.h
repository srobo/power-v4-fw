#include <stdbool.h>

// Configure a 1Khz timer for us to make decisions about the passage of time.
void clock_isr(void);
bool clock_tick(void);
