#include <stdint.h>
#include <stdbool.h>

#pragma once

#define PIEZO_BUFFER_LEN 32

void piezo_init(void);

void piezo_toggle(void);

bool piezo_recv(uint32_t size, uint8_t *data);
