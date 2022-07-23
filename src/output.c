#include "output.h"
#include "global_vars.h"

#include <libopencm3/stm32/gpio.h>

#define REG_TRIM_PORT GPIOC
#define REG_TRIM_PIN GPIO12

// 6 outputs and the 5V reg
static const uint32_t OUTPUT_PORT[7] = {GPIOB,  GPIOB,  GPIOC, GPIOC, GPIOC, GPIOC, GPIOB};
static const uint32_t OUTPUT_PIN[7]  = {GPIO10, GPIO11, GPIO6, GPIO7, GPIO8, GPIO9, GPIO5};

static const uint32_t OUTPUT_CSDIS_PIN[4] = {GPIO0, GPIO1, GPIO2, GPIO3};

uint16_t output_current[7] = {0};  // reg value here is unused

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

bool enable_output(output_t out, bool enable) {
    /// TODO add disabled logic

    if (enable) {
        gpio_set(OUTPUT_PORT[out], OUTPUT_PIN[out]);
    } else {
        gpio_clear(OUTPUT_PORT[out], OUTPUT_PIN[out]);
    }
    return true;
}

bool output_enabled(output_t out) {
    return (gpio_get(OUTPUT_PORT[out], OUTPUT_PIN[out]))?true:false;
}
