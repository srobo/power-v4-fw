#include "systick.h"
#include "global_vars.h"
#include "i2c.h"
#include "adc.h"
#include "output.h"
#include "fan.h"
#include "button.h"
#include "led.h"
#include "buzzer.h"

#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>

INA219_meas_t battery = {0};
INA219_meas_t reg_5v = {0};
int16_t board_temp = 0;

uint8_t systick_slow_tick = 0;
uint16_t systick_temp_tick = 0;
uint8_t current_phase = 0;

void systick_init(void) {
    // Generate a 1ms systick interrupt
    // 72MHz / 8 => 9000000 counts per second
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);

    // 9000000/9000 = 1000 overflows per second
    // SysTick interrupt every N clock pulses
    systick_set_reload(9000);

    systick_interrupt_enable();

    // Start counting.
    systick_counter_enable();
}

void sys_tick_handler(void) {
    // Every 20 ms read values from INA219 current sensors
    if (++systick_slow_tick == 20) {
        // if watchdog tripped re-init INA219's
        if (i2c_timed_out) {
            // reset watchdog
            reset_i2c_watchdog();
            init_i2c_sensors();
        }
        // Read INA219's
        battery = measure_current_sense(BATTERY_SENSE_ADDR);
        battery.current *= 10;  // convert to 1mA LSB
        reg_5v = measure_current_sense(REG_SENSE_ADDR);

        // Check UVLO
        handle_uvlo();
        systick_slow_tick = 0;
    }
    // Every 1s read temp sensor
    if (++systick_temp_tick == 1000) {
        // Read temp sense
        board_temp = adc_to_temp(get_adc_measurement(TEMP_SENSE_CHANNEL));

        // Set fan
        if(board_temp > FAN_THRESHOLD) {
            fan_enable(true);
        } else if (board_temp < FAN_THRESHOLD) {  // 2 degree hysteresis
            fan_enable(false);
        }

        handle_led_flash();
        systick_temp_tick = 0;
    }

    buzzer_tick();

    sample_buttons();

    // Read next ADC phase
    read_next_current_phase(current_phase);
    current_phase++;
    current_phase %= 4;

    // Check current limits
    detect_overcurrent();
}
