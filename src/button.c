#include "button.h"
#include "global_vars.h"

bool int_button_pressed = false;
bool ext_button_pressed = false;

void button_init(void) {
    gpio_set(INT_BTN_PORT, INT_BTN_PIN); // Pull-up
    gpio_set_mode(INT_BTN_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, INT_BTN_PIN);
    gpio_set(EXT_BTN_PORT, EXT_BTN_PIN); // Pull-up
    gpio_set_mode(EXT_BTN_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, EXT_BTN_PIN);
}

bool button_int_read(void) {
    return gpio_get(INT_BTN_PORT, INT_BTN_PIN);
}
bool button_ext_read(void) {
    return gpio_get(EXT_BTN_PORT, EXT_BTN_PIN);
}
