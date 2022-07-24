#include "adc.h"
#include "output.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/gpio.h>


static void adcx_init(uint32_t ADC) {
    // Make sure the ADC doesn't run during config.
    adc_power_off(ADC);

    // We configure everything for one single conversion.
    adc_set_single_conversion_mode(ADC);
    adc_disable_external_trigger_regular(ADC);
    adc_set_right_aligned(ADC);

    adc_set_sample_time_on_all_channels(ADC, ADC_SMPR_SMP_28DOT5CYC);

    adc_power_on(ADC);

    // Wait for ADC starting up.
    for (uint32_t i = 0; i < 800000; i++){__asm__("nop");}

    adc_reset_calibration(ADC);
    adc_calibrate(ADC);
}

void adc_init(void) {
    rcc_periph_clock_enable(RCC_ADC1);
    rcc_periph_clock_enable(RCC_ADC2);

    adcx_init(ADC1);
    adcx_init(ADC2);

    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, (GPIO0|GPIO1));
	gpio_set_mode(GPIOC, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO5);
}

uint16_t get_adc_measurement(uint8_t channel) {
    // Select the channel we want to convert.
    uint8_t channel_array[1] = {channel};
    adc_set_regular_sequence(ADC1, 1, channel_array);

    // Start the conversion directly (not trigger mode).
    adc_start_conversion_direct(ADC1);

    // Wait for end of conversion.
    while (!(adc_eoc(ADC1)));

    return (uint16_t)(adc_read_regular(ADC1) & 0xffff);
}

void read_next_current_phase(uint8_t phase) {
    // Configure CSDIS pins
    setup_current_phase(phase);

    // Select the channel we want to convert.
    uint8_t adc1_channel_array[1] = {0};
    uint8_t adc2_channel_array[1] = {1};
    adc_set_regular_sequence(ADC1, 1, adc1_channel_array);
    adc_set_regular_sequence(ADC2, 1, adc2_channel_array);

    // Start the conversion directly (not trigger mode).
    adc_start_conversion_direct(ADC1);
    adc_start_conversion_direct(ADC2);

    // Wait for end of conversion.
    while (!(adc_eoc(ADC1) && adc_eoc(ADC2)));

    uint16_t res1 = (uint16_t)(adc_read_regular(ADC1) & 0xffff);
    uint16_t res2 = (uint16_t)(adc_read_regular(ADC2) & 0xffff);

    save_current_values(phase, adc_to_current(res1), adc_to_current(res2));
}

int16_t adc_to_temp(uint16_t adc_val) {
    // ADC LSB: 3.3/(2^12) = 805.66e-6 V/bit
    // V(0deg) = 0.4 V
    // Offset: 0.4/805.66e-6 = 496.4 bit
    // Tc = 19.5 mV/deg
    // 805.66e-6/19.5e-3 = 0.0413 deg/bit
    // 19.5e-3/805.66e-6 = 24.2 bit/deg = 6656/275
    int offset = 496;

    // We have a hardware divider, we may as well use it
    int32_t raw_val = ((int32_t)adc_val - offset);
    return (int16_t)((raw_val * 275) / 6656);
}
uint16_t adc_to_current(uint16_t adc_val) {
    // ADC LSB: 3.3/(2^12) = 805.66e-6 V/bit
    // (1/5100)*560 = 109.8e-3 V/A
    // 805.66/109800 * 1e3 = 7.34 mA/bit = 7 + 9671/28672

    return (7 * adc_val) + (uint16_t)(((uint32_t)adc_val * 9671) / 28672);
}



