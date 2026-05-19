#include <stdint.h>

namespace {
constexpr uint32_t POWER_GPREGRET = 0x4000051CUL;
constexpr uint32_t SCB_AIRCR = 0xE000ED0CUL;
constexpr uint32_t SYSRESETREQ = 0x05FA0004UL;
constexpr uint32_t DFU_MAGIC_SERIAL_ONLY_RESET = 0x4EUL;

inline volatile uint32_t &reg32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}

void returnToSerialDfuPreinit() {
    reg32(POWER_GPREGRET) = DFU_MAGIC_SERIAL_ONLY_RESET;
    reg32(SCB_AIRCR) = SYSRESETREQ;
    while (true) {
    }
}

using PreinitFunc = void (*)();
__attribute__((section(".preinit_array"), used)) PreinitFunc g_returnToSerialDfuPreinit = returnToSerialDfuPreinit;
}

void setup() {
}

void loop() {
}
