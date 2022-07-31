#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {OUT_H0=0, OUT_H1, OUT_L0, OUT_L1, OUT_L2, OUT_L3, OUT_5V} output_t;

extern uint16_t output_current[];
extern bool output_inhibited[];

void outputs_init(void);

void setup_current_phase(uint8_t phase);
void save_current_values(uint8_t phase, uint16_t current1, uint16_t current2);

bool enable_output(output_t out, bool enable);
bool output_enabled(output_t out);

void handle_uvlo(void);
void detect_overcurrent(void);

void disable_all_outputs(bool disable_brain);

void usb_reset_callback(void);
