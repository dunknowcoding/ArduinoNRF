#pragma once

// Firmware-side GDB stub over USB CDC. Transport is bound to the service/maintenance
// CDC interface; do not multiplex application Serial printf on the same interface.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct NrfGdbRegs {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r12;
    uint32_t sp;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
};

class NrfGdbStubClass {
public:
    void init();
    void poll();
    bool enabled() const;
    bool active() const;
    void handleException(uint32_t *stack, uint32_t excReturn, uint32_t exceptionNumber);

private:
    // Hardware breakpoint slot. nRF52840 (Cortex-M4 r0p1) provides 6 instruction
    // comparators in the FPB unit, which we use to satisfy GDB's Z0 packets.
    struct FpbSlot {
        bool inUse;
        uint32_t address;
    };
    static constexpr size_t kFpbMaxSlots = 6;

    void capture(uint32_t *stack, uint32_t excReturn, uint32_t exceptionNumber);
    void applyRegs();
    void serve();
    void clearFaults();
    void continueFrom(const char *packet);
    void sendStop();
    void sendPacket(const char *payload);
    bool readPacket(char *packet, size_t capacity);
    void reply(char *packet, char *response, size_t capacity);

    // Z0/z0 software breakpoint backing (implemented as Cortex-M FPB hw bps).
    void fpbInitOnce();
    bool fpbInsert(uint32_t address);
    bool fpbRemove(uint32_t address);
    void fpbDisableAll();

    // Single step via DEMCR.MON_STEP. Returns from exception with the bit set;
    // the next instruction re-enters DebugMon_Handler.
    void enableMonStep();
    void disableMonStep();

    // M packet: writes data into RAM only. Flash writes are refused (E03)
    // because GDB doesn't need them when Z0 is supported.
    bool memoryWritable(uint32_t address, uint32_t length) const;
    bool writeMemoryBytes(uint32_t address, const uint8_t *data, uint32_t length);

    bool initialized_ = false;
    bool active_ = false;
    bool noAckMode_ = false;
    bool stopPending_ = false;
    bool fpbReady_ = false;
    bool usbIrqWasEnabled_ = false;
    uint8_t signal_ = 5;
    uint32_t excReturn_ = 0;
    uint32_t exceptionNumber_ = 0;
    uint32_t *stackFrame_ = nullptr;
    NrfGdbRegs regs_ = {0};
    FpbSlot fpbSlots_[kFpbMaxSlots] = {};
};

NrfGdbStubClass &nrfGdbStub();
uint8_t nrfGdbStubBreadcrumb();
void nrfGdbStubClearBreadcrumb();

extern "C" void nrfGdbStubHandleExceptionC(uint32_t *stack, uint32_t excReturn, uint32_t exceptionNumber);
