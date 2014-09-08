#ifndef __LED_H
#define __LED_H

#include <libopencm3/stm32/gpio.h>

#define LED_RUN GPIO8
#define LED_ERROR GPIO9

void led_init(void);

#define led_set(led) gpio_clear(GPIOB, led)
#define led_clear(led) gpio_set(GPIOB, led)
#define led_toggle(led) gpio_toggle(GPIOB, led)

#define led_toggle_flat() gpio_toggle(GPIOD, GPIO2);

#endif /* __LED_H */
