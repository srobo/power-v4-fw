#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {OUT_H0=0, OUT_H1, OUT_L0, OUT_L1, OUT_L2, OUT_L3, OUT_5V} output_t;
typedef struct {
    uint16_t H0;
    uint16_t H1;
    uint16_t L0;
    uint16_t L1;
    uint16_t L2;
    uint16_t L3;
} out_current_t;


void outputs_init(void);

void setup_current_phase(uint8_t phase);
void save_current_values(uint8_t phase, uint16_t current1, uint16_t current2);

bool enable_output(output_t out, bool enable);
