#pragma once

#include <stdint.h>

#define LED_RUN GPIO8
#define LED_ERROR GPIO9
#define LED_FLAT GPIO2
#define LED_STATH0 GPIO10
#define LED_STATH1 GPIO11
#define LED_STATL0 GPIO12
#define LED_STATL1 GPIO13
#define LED_STATL2 GPIO14
#define LED_STATL3 GPIO15


void led_init(void);

void set_led(uint32_t pin);
void clear_led(uint32_t pin);
void toggle_led(uint32_t pin);
uint8_t get_led_state(uint32_t pin);

void set_led_flash(uint32_t pin);
void handle_led_flash(void);
