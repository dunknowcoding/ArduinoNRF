#include "host/ble_hs.h"

int nrf_nimble_host_headers_smoke(void) {
    return BLE_HS_FOREVER != 0 && sizeof(struct ble_hs_cfg) > 0;
}