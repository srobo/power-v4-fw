#include "buzzer.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/gpio.h>


volatile uint32_t buzzer_ticks_remaining = 0;
uint16_t buzzer_frequency = 0;

void buzzer_init(void) {
    // Enable TIM3 clock
    rcc_periph_clock_enable(RCC_TIM3);

    // Reset TIM3 peripheral.
    rcc_periph_reset_pulse(RST_TIM3);
    timer_set_prescaler(TIM3, 72);  // 72Mhz -> 1Mhz
    timer_set_period(TIM1, UINT16_MAX);  // default value, will be overridden

    // Up counting, edge triggered no divider
    timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_continuous_mode(TIM3);

    // Toggle output when counter == compare (0)
    timer_set_oc_mode(TIM3, TIM_OC3, TIM_OCM_TOGGLE);
    timer_set_oc_value(TIM3, TIM_OC3, 0);
    timer_enable_oc_output(TIM3, TIM_OC3);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO0);
}

void buzzer_note(uint16_t freq, uint32_t duration) {
    if ((freq == 0) || (duration == 0)) {
        buzzer_stop();
        return;
    }
    if (freq > 10000) {
        // bound frequency to 10kHz, the resonator isn't designed for above this
        freq = 10000;
    }

    // Set timer period for frequency
    timer_set_period(TIM3, (1000000 / 2) / freq);
    buzzer_frequency = freq;

    // Enable timer
    timer_enable_counter(TIM3);
    buzzer_ticks_remaining = duration;
}

void buzzer_stop(void) {
    // Disable timer
    timer_disable_counter(TIM3);
    timer_set_counter(TIM3, 0);

    buzzer_ticks_remaining = 0;
}

bool buzzer_running(void) {
    return (buzzer_ticks_remaining != 0);
}

void buzzer_tick(void) {
    if (buzzer_ticks_remaining != 0) {
        if (--buzzer_ticks_remaining == 0) {
            buzzer_stop();
        }
    }
}

uint16_t buzzer_get_freq(void) {
    if (buzzer_running()) {
        return buzzer_frequency;
    } else {
        return 0;
    }
}
uint32_t buzzer_remaining(void) {
    return buzzer_ticks_remaining;
}
