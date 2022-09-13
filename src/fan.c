#include "fan.h"

void fan_init(void) {
    gpio_clear(FAN_PORT, FAN_PIN);
    gpio_set_mode(FAN_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, FAN_PIN);
}

void fan_enable(const bool enable) {
    if (enable) {
        gpio_set(FAN_PORT, FAN_PIN);
    } else {
        gpio_clear(FAN_PORT, FAN_PIN);
    }
}

bool fan_running(void) {
    return gpio_get(FAN_PORT, FAN_PIN);
}
