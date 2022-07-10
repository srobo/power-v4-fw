#include "adc.h"
#include "output.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>


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

    save_current_values(phase, res1, res2);
}
