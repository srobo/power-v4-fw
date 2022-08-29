#include "output.h"
#include "global_vars.h"
#include "led.h"
#include "fan.h"
#include "cdcacm.h"
#include "buzzer.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/iwdg.h>

#define REG_TRIM_PORT GPIOC
#define REG_TRIM_PIN GPIO12

// 6 outputs and the 5V reg
static const uint32_t OUTPUT_PORT[7] = {GPIOB,  GPIOB,  GPIOC, GPIOC, GPIOC, GPIOC, GPIOB};
static const uint32_t OUTPUT_PIN[7]  = {GPIO10, GPIO11, GPIO6, GPIO7, GPIO8, GPIO9, GPIO5};

static const uint32_t OUTPUT_CSDIS_PIN[4] = {GPIO0, GPIO1, GPIO2, GPIO3};

// In ms, ADC delay is in steps of 4, batt and reg are in multiples of 20
uint16_t ADC_OVERCURRENT_DELAY = 4;
uint16_t BATT_OVERCURRENT_DELAY = 20;
uint16_t REG_OVERCURRENT_DELAY = 20;

uint8_t overcurrent_delay[8] = {0};
uint16_t output_current[7] = {0};  // reg value here is unused
bool output_inhibited[7] = {0};

void outputs_init(void) {
    // SMPS
    gpio_set(REG_TRIM_PORT, REG_TRIM_PIN);
    gpio_set_mode(REG_TRIM_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, REG_TRIM_PIN);

    for (uint8_t i = 0; i < 7; i++) {
        gpio_clear(OUTPUT_PORT[i], OUTPUT_PIN[i]);
        gpio_set_mode(OUTPUT_PORT[i], GPIO_MODE_OUTPUT_10_MHZ,
                      GPIO_CNF_OUTPUT_PUSHPULL, OUTPUT_PIN[i]);
    }
    // Configure current-sense disable pins, initially all disabled
    for (int i = 0; i < 4; i++) {
        gpio_clear(GPIOC, OUTPUT_CSDIS_PIN[i]);
        gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ,
                      GPIO_CNF_OUTPUT_PUSHPULL, OUTPUT_CSDIS_PIN[i]);
        gpio_set(GPIOC, OUTPUT_CSDIS_PIN[i]);
    }
    setup_current_phase(0);
}

void setup_current_phase(uint8_t phase) {
    // Disable all current-sense pins
    gpio_set(GPIOC, 0x000f);

    if (phase >= 4) {
        return;
    }

    // Enable selected phase
    gpio_clear(GPIOC, OUTPUT_CSDIS_PIN[phase]);
}
void save_current_values(uint8_t phase, uint16_t current1, uint16_t current2) {
    uint16_t total_current = current1 + current2;
    switch (phase) {
        case 0:  // H0
            output_current[OUT_H0] = total_current;
            break;
        case 1:  // H1
            output_current[OUT_H1] = total_current;
            break;
        case 2:  // L0 & L1
            output_current[OUT_L0] = current1;
            output_current[OUT_L1] = current2;
            break;
        case 3:  // L2 & L3
            output_current[OUT_L2] = current1;
            output_current[OUT_L3] = current2;
            break;
    }
}

static void _enable_output(output_t out, bool enable) {
    if (enable) {
        gpio_set(OUTPUT_PORT[out], OUTPUT_PIN[out]);
    } else {
        gpio_clear(OUTPUT_PORT[out], OUTPUT_PIN[out]);
    }
}
bool enable_output(output_t out, bool enable) {
    if (output_inhibited[out]) {
        // Output has had an overcurrent and is disabled
        return false;
    }

    _enable_output(out, enable);
    return true;
}

bool output_enabled(output_t out) {
    return (gpio_get(OUTPUT_PORT[out], OUTPUT_PIN[out]))?true:false;
}

void set_overcurrent(output_t out, bool overcurrent) {
    if (overcurrent) {
        // disable channel
        _enable_output(out, false);
        output_inhibited[out] = true;
        // set channel error LED
        switch (out) {
            case OUT_H0:
                set_led(LED_STATH0); break;
            case OUT_H1:
                set_led(LED_STATH1); break;
            case OUT_L0:
                set_led(LED_STATL0); break;
            case OUT_L1:
                set_led(LED_STATL1); break;
            case OUT_L2:
                set_led(LED_STATL2); break;
            case OUT_L3:
                set_led(LED_STATL3); break;
            default: break;
        }
    } else {
        output_inhibited[out] = false;
        // clear channel error LED
        switch (out) {
            case OUT_H0:
                clear_led(LED_STATH0); break;
            case OUT_H1:
                clear_led(LED_STATH1); break;
            case OUT_L0:
                clear_led(LED_STATL0); break;
            case OUT_L1:
                clear_led(LED_STATL1); break;
            case OUT_L2:
                clear_led(LED_STATL2); break;
            case OUT_L3:
                clear_led(LED_STATL3); break;
            default: break;
        }
    }
}

void handle_uvlo(void) {
    // Test if global voltage is below 10.2V
    if ((battery.success) && (battery.voltage < 10200)) {
        disable_all_outputs(true);
        set_led(LED_FLAT);

        // Disable systick & USB
        systick_counter_disable();
        usb_deinit();

        // Disable fan
        fan_enable(false);

        while (1) {
            // flash flat LED
            toggle_led(LED_FLAT);

            for (unsigned int j = 0; j < 25; j++) {
                delay(20);
                iwdg_reset();
            }
        }

    }
}

void detect_overcurrent(void) {
    // Test individual output currents
    for (output_t out=OUT_H0; out <= OUT_L3; out++) {
        uint16_t current_limit = (out <= OUT_H1) ? 20000 : 10000;

        if (output_current[out] > current_limit) {
            overcurrent_delay[out]++;
            if (overcurrent_delay[out] > ADC_OVERCURRENT_DELAY) {
                // disable channel
                set_overcurrent(out, true);
            }
        } else {
            overcurrent_delay[out] = 0;
        }
    }
    if ((reg_5v.success) && (reg_5v.current > 2000)) {
        overcurrent_delay[OUT_5V]++;
        if (overcurrent_delay[OUT_5V] > REG_OVERCURRENT_DELAY) {
            // disable channel
            set_overcurrent(OUT_5V, true);
        }
    } else {
        overcurrent_delay[OUT_5V] = 0;
    }

    // Test global current
    // Also test combined output current
    int total_current = 0;
    for (output_t out=OUT_H0; out < OUT_5V; out++) {
        total_current += output_current[out];
    }
    if (reg_5v.success) {
        total_current += reg_5v.current;
    }
    if (
        (total_current > 30000)
        || ((battery.success) && (battery.current > 30000))
    ) {
        overcurrent_delay[7]++;
        if (overcurrent_delay[7] > BATT_OVERCURRENT_DELAY) {
            disable_all_outputs(true);

            // Disable systick & USB
            systick_counter_disable();
            usb_deinit();

            // Enable fan
            fan_enable(true);

            while (1) {
                // sound buzzer
                // buzzer duration is meaningless now systick is stopped
                buzzer_note(2000, 1000);
                for (unsigned int j = 0; j < 50; j++) {
                    delay(20);
                    iwdg_reset();
                }

                // stop buzzer
                buzzer_stop();
                for (unsigned int j = 0; j < 25; j++) {
                    delay(20);
                    iwdg_reset();
                }

                // test battery undervoltage
                // if watchdog tripped re-init INA219's
                if (i2c_timed_out) {
                    // reset watchdog
                    reset_i2c_watchdog();
                    init_i2c_sensors();
                }
                // Read INA219
                battery = measure_current_sense(BATTERY_SENSE_ADDR);
                battery.current *= 10;  // convert to 1mA LSB
                handle_uvlo();
            }
        }
    } else {
        overcurrent_delay[7] = 0;
    }
}

void disable_all_outputs(bool disable_brain) {
    // Inhibit all outputs to prevent them being re-enabled
    for (output_t out=OUT_H0; out <= OUT_5V; out++) {
        if ((!disable_brain) && (out == OUT_L2)) {continue;}
        _enable_output(out, false);
        output_inhibited[out] = true;
    }

    // Disable run LED
    clear_led(LED_RUN);
    // Disable buzzer
    buzzer_stop();

    set_led(LED_ERROR);
}

void usb_reset_callback(void) {
    // Switch off all outputs except brain
    for (uint8_t i = 0; i < 7; i++) {
        if (i != BRAIN_OUTPUT) {
            enable_output(i, false);
        }
    }

    // Disable buzzer
    buzzer_stop();

    set_led(LED_RUN);
    set_led(LED_ERROR);
}
