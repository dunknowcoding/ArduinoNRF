#pragma once

#include <stdint.h>

void nrfStartLfclk();
void nrfStartHfclk();
bool nrfLfclkRunning();
bool nrfHfclkRunning();
uint32_t nrfDeviceIdWord(uint8_t index);
