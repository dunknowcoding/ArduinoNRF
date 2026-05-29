// Minimal nrfx configuration for the ArduinoNRF NimBLE controller probe.
#ifndef NRFX_CONFIG_H__
#define NRFX_CONFIG_H__

#ifndef NRF52840_XXAA
#define NRF52840_XXAA 1
#endif

#ifndef NRF52_SERIES
#define NRF52_SERIES 1
#endif

#ifndef NRFX_PRS_ENABLED
#define NRFX_PRS_ENABLED 0
#endif

#ifndef NRFX_CONFIG_API_VER_MAJOR
#define NRFX_CONFIG_API_VER_MAJOR 4
#endif

#ifndef NRFX_CONFIG_API_VER_MINOR
#define NRFX_CONFIG_API_VER_MINOR 3
#endif

#ifndef NRFX_CONFIG_API_VER_MICRO
#define NRFX_CONFIG_API_VER_MICRO 0
#endif

#include "nrfx_config_common.h"

#endif