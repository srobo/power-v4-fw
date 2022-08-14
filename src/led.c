#include "led.h"

#include <libopencm3/stm32/gpio.h>

bool led_run_flashing = false;
bool led_error_flashing = false;
bool led_flat_flashing = false;


void led_init(void) {
    // set pins B8-9, B12-15, C10-11 & D2 to outputs
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_10_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, 0xf300);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, 0x0C00);
    gpio_set_mode(GPIOD, GPIO_MODE_OUTPUT_10_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, LED_FLAT);

    // set LEDs to off state
    gpio_set(GPIOB, LED_RUN|LED_ERROR);  // active low LEDs
    gpio_set(GPIOD, LED_FLAT);  // active low LEDs
    gpio_clear(GPIOC, LED_STATH0|LED_STATH1);  // active high LEDs
    gpio_clear(GPIOB, 0xf000);  // active high LEDs
}

void set_led(uint32_t pin) {
    switch (pin) {
        // active-low LEDs
        case LED_RUN:
        case LED_ERROR:
            if (pin == LED_RUN) {
                led_run_flashing = false;
            } else {
                led_error_flashing = false;
            }
            gpio_clear(GPIOB, pin);
            break;

        case LED_FLAT:
            led_flat_flashing = false;
            gpio_clear(GPIOD, pin);
            break;

        // active-high LEDs
        case LED_STATH0:
        case LED_STATH1:
            gpio_set(GPIOC, pin);
            break;
        case LED_STATL0:
        case LED_STATL1:
        case LED_STATL2:
        case LED_STATL3:
            gpio_set(GPIOB, pin);
            break;
    }
}

void clear_led(uint32_t pin) {
    switch (pin) {
        // active-low LEDs
        case LED_RUN:
        case LED_ERROR:
            if (pin == LED_RUN) {
                led_run_flashing = false;
            } else {
                led_error_flashing = false;
            }
            gpio_set(GPIOB, pin);
            break;

        case LED_FLAT:
            led_flat_flashing = false;
            gpio_set(GPIOD, pin);
            break;

        // active-high LEDs
        case LED_STATH0:
        case LED_STATH1:
            gpio_clear(GPIOC, pin);
            break;
        case LED_STATL0:
        case LED_STATL1:
        case LED_STATL2:
        case LED_STATL3:
            gpio_clear(GPIOB, pin);
            break;
    }
}


void toggle_led(uint32_t pin) {
    switch (pin) {
        case LED_RUN:
        case LED_ERROR:
            if (pin == LED_RUN) {
                led_run_flashing = false;
            } else {
                led_error_flashing = false;
            }
            gpio_toggle(GPIOB, pin);
            break;

        case LED_FLAT:
            led_flat_flashing = false;
            gpio_toggle(GPIOD, pin);
            break;

        case LED_STATH0:
        case LED_STATH1:
            gpio_toggle(GPIOC, pin);
            break;
        case LED_STATL0:
        case LED_STATL1:
        case LED_STATL2:
        case LED_STATL3:
            gpio_toggle(GPIOB, pin);
            break;
    }
}

void set_led_flash(uint32_t pin) {
    switch (pin) {
        case LED_RUN:
            led_run_flashing = true;
            break;
        case LED_ERROR:
            led_error_flashing = true;
            break;
        case LED_FLAT:
            led_flat_flashing = true;
            break;

        default:
            break;
    }
}
void handle_led_flash(void) {
    if (led_run_flashing) {
        gpio_toggle(GPIOB, LED_RUN);
    }
    if (led_error_flashing) {
        gpio_toggle(GPIOB, LED_ERROR);
    }
    if (led_flat_flashing) {
        gpio_toggle(GPIOB, LED_FLAT);
    }
}