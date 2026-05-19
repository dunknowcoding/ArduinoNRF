#pragma once

#include <stdint.h>

bool nrfPeripheralTake(uint32_t base, const void *owner);
bool nrfPeripheralOwnerIs(uint32_t base, const void *owner);
void nrfPeripheralRelease(uint32_t base, const void *owner);