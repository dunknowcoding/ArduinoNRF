#pragma once

// NrfGdbStub - the DEBUG submodule of TaichiUSB (the ArduinoNRF self-developed
// USB device stack; see TaichiUsb.h). This firmware-side GDB Remote Serial
// Protocol stub rides TaichiUSB's maintenance/service CDC interface and depends
// on the stub-halted hooks the device core exposes for it (NrfUsbdDriver::
// setStubHalted / serviceHaltedTouch / drainServiceDataOut /
// kickServiceDataIn) so single-cable debugging keeps working while the target is
// halted in DebugMon. Transport is bound to the service CDC only; do not
// multiplex application Serial printf on the same interface.

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

    // Async break (IDE 2 "Pause"). While the target runs free, the stub is
    // dormant -- nothing reads the service CDC. yield() calls this so a host
    // Ctrl-C/0x03 byte is noticed and turned into a pended DebugMon exception
    // that halts the target. No-op unless a debug session has resumed us.
    void serviceAsyncBreak();

private:
    // Hardware breakpoint slot. nRF52840 (Cortex-M4 r0p1) provides 6 instruction
    // comparators in the FPB unit, which we use to satisfy GDB's Z0 packets.
    struct FpbSlot {
        bool inUse;
        uint32_t address;
    };
    static constexpr size_t kFpbMaxSlots = 6;

    // Data watchpoint slot, backed by a Cortex-M4 DWT comparator (4 on this
    // part). Satisfies GDB's Z2/Z3/Z4 (write/read/access) packets.
    struct DwtSlot {
        bool inUse;
        uint32_t address;
        uint8_t function; // DWT_FUNCTION FUNCTION value (5 read, 6 write, 7 access)
    };
    static constexpr size_t kDwtMaxSlots = 4;

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

    // Z2/Z3/Z4 data watchpoint backing (Cortex-M4 DWT comparators).
    void dwtInitOnce();
    bool dwtInsert(uint32_t function, uint32_t address, uint32_t kind);
    bool dwtRemove(uint32_t function, uint32_t address);
    void dwtDisableAll();
    // After a halt, work out whether a DWT watchpoint fired and which address;
    // fills watchHit*/returns true so sendStop() can emit the watch stop reply.
    bool detectWatchpointHit();

    // "monitor reset" (qRcmd) warm restart: re-point SP/PC at the app's reset
    // vector so the next resume re-runs C startup (re-inits .data/.bss, setup()).
    void performWarmRestart();

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
    // Only volunteer a stop reply ($Sxx) when gdb is actually waiting for one,
    // i.e. after it resumed us with c/s/vCont. On the INITIAL attach gdb drives
    // the handshake (qSupported, ?, ...) and an unsolicited stop shifts every
    // response by one packet, desyncing the session. Set when we resume.
    bool expectStopReply_ = false;
    bool fpbReady_ = false;
    bool dwtReady_ = false;
    bool usbIrqWasEnabled_ = false;
    // Set once gdb has resumed us at least once (real session live). Gates the
    // async-break peek so we never pend a halt before a session exists.
    volatile bool sessionLive_ = false;
    // Raised by serviceAsyncBreak() when it consumes a host 0x03 and pends a
    // DebugMon halt; capture() turns it into a SIGINT stop.
    volatile bool pendingAsyncBreak_ = false;
    // Filled by detectWatchpointHit(): which data address tripped and its kind
    // ('w' write / 'r' read / 'a' access) so sendStop() can emit T05<kind>:addr;.
    bool watchHit_ = false;
    char watchHitKind_ = 'a';
    uint32_t watchHitAddr_ = 0;
    uint8_t signal_ = 5;
    uint32_t excReturn_ = 0;
    uint32_t exceptionNumber_ = 0;
    uint32_t *stackFrame_ = nullptr;
    NrfGdbRegs regs_ = {0};
    FpbSlot fpbSlots_[kFpbMaxSlots] = {};
    DwtSlot dwtSlots_[kDwtMaxSlots] = {};
};

NrfGdbStubClass &nrfGdbStub();
uint8_t nrfGdbStubBreadcrumb();
void nrfGdbStubClearBreadcrumb();

extern "C" void nrfGdbStubHandleExceptionC(uint32_t *stack, uint32_t excReturn, uint32_t exceptionNumber);
