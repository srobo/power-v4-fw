#include "systick.h"

#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>

uint8_t systick_slow_tick = 0;
uint16_t systick_temp_tick = 0;

void systick_init(void) {
    // Generate a 0.25ms systick interrupt
    // 72MHz / 8 => 9000000 counts per second
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);

    // 9000000/2250 = 4000 overflows per second
    // SysTick interrupt every N clock pulses
    systick_set_reload(2250);

    systick_interrupt_enable();

    // Start counting.
    systick_counter_enable();
}

void sys_tick_handler(void) {
    // Every 20 ms read values from INA219 current sensors
    if (++systick_slow_tick == 80) {
        /// TODO handle i2c timeout
        /// TODO Read INA219's

        /// TODO check current limits
        /// TODO check UVLO
        systick_slow_tick = 0;
    }
    // Every 1s read temp sensor
    if (++systick_temp_tick == 4000) {
        /// TODO Read temp sense
        systick_temp_tick = 0;
        /// TODO set fan speed
    }

    /// TODO read next ADC step
}
