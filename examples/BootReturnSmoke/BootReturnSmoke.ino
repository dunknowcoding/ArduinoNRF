#include <Arduino.h>

namespace {
constexpr uint32_t POWER_GPREGRET = 0x4000051CUL;
constexpr uint32_t USBD_BASE = 0x40027000UL;
constexpr uint32_t USBD_ENABLE = 0x500UL;
constexpr uint32_t USBD_USBPULLUP = 0x504UL;
constexpr uint32_t SCB_AIRCR = 0xE000ED0CUL;
constexpr uint32_t SYSRESETREQ = 0x05FA0004UL;
constexpr uint32_t DFU_MAGIC_SERIAL_ONLY_RESET = 0x4EUL;

inline volatile uint32_t &reg32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}
}

void setup() {
    delay(500);
    reg32(USBD_BASE + USBD_USBPULLUP) = 0UL;
    reg32(USBD_BASE + USBD_ENABLE) = 0UL;
    delay(50);
    reg32(POWER_GPREGRET) = DFU_MAGIC_SERIAL_ONLY_RESET;
    reg32(SCB_AIRCR) = SYSRESETREQ;
    while (true) {
    }
}

void loop() {
}
