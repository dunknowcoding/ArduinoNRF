// USB GDB Remote Protocol stub: I/O is wired to the maintenance/service CDC only
// (nrfUsbd driver path). User Sketch Serial uses the separate user CDC when enabled.
#include "NrfGdbStub.h"

#include <string.h>

#include "NrfSystem.h"
#include "NrfUsbd.h"
#include "USBDevice.h"

extern "C" {
uint32_t nrf_gdb_stub_callee_regs[8] = {0};
}

namespace {
// ARMv7-M System Control Block / Debug Control Block registers.
constexpr uint32_t DEMCR = 0xE000EDFCUL;
constexpr uint32_t DEMCR_MON_EN = 0x00010000UL;   // Bit 16: enable DebugMon exception.
constexpr uint32_t DEMCR_MON_STEP = 0x00040000UL; // Bit 18: step a single instruction after exit.
constexpr uint32_t DEMCR_TRCENA = 0x01000000UL;   // Bit 24: enable DWT/FPB tracing block.
constexpr uint32_t SHCSR = 0xE000ED24UL;
constexpr uint32_t SHCSR_MEMFAULTENA = 0x00010000UL;
constexpr uint32_t SHCSR_BUSFAULTENA = 0x00020000UL;
constexpr uint32_t SHCSR_USGFAULTENA = 0x00040000UL;
constexpr uint32_t CFSR = 0xE000ED28UL;
constexpr uint32_t HFSR = 0xE000ED2CUL;
constexpr uint32_t DFSR = 0xE000ED30UL;
constexpr uint32_t DFSR_HALTED = 0x00000001UL;    // Halt request (BKPT or step).
constexpr uint32_t DFSR_BKPT = 0x00000002UL;      // BKPT or FPB instruction match.

// Cortex-M Flash Patch and Breakpoint unit. Provides up to 6 instruction
// comparators that halt the CPU into DebugMon_Handler when PC matches a flash
// address. We use this for GDB's Z0 packet so we never have to NVMC-erase
// pages just to plant a BKPT into running code.
constexpr uint32_t FP_CTRL = 0xE0002000UL;
constexpr uint32_t FP_CTRL_ENABLE = 0x00000001UL;
constexpr uint32_t FP_CTRL_KEY = 0x00000002UL;    // Required for any FP_CTRL write.
constexpr uint32_t FP_COMP_BASE = 0xE0002008UL;
constexpr uint32_t FP_COMP_ENABLE = 0x00000001UL;
constexpr uint32_t FP_COMP_REPLACE_LO = 0x40000000UL; // Match on 4-byte-aligned PC.
constexpr uint32_t FP_COMP_REPLACE_HI = 0x80000000UL; // Match on +2-aligned half-word.
constexpr uint32_t FP_COMP_ADDR_MASK = 0x1FFFFFFCUL;

// NVIC interrupt set/clear-enable registers. We only need to mask USBD_IRQ
// (number 39 on nRF52840) so the stub can poll the USB controller without
// racing the ISR. Bit (irq mod 32) of register (irq / 32).
constexpr uint32_t NVIC_ISER_BASE = 0xE000E100UL;
constexpr uint32_t NVIC_ICER_BASE = 0xE000E180UL;
constexpr uint32_t USBD_NVIC_IRQ = 39UL;
constexpr uint32_t USBD_NVIC_REG_OFFSET = (USBD_NVIC_IRQ / 32U) * 4U;
constexpr uint32_t USBD_NVIC_BIT = 1UL << (USBD_NVIC_IRQ % 32U);
constexpr uint32_t POWER_BASE = 0x40000000UL;
constexpr uint32_t POWER_GPREGRET = 0x51CUL;

// Hardware watchdog (WDT). A sketch may arm it before hitting a breakpoint
// (the UsbGdbStubBreakpoint example does, as a brick-recovery safety). Once
// started the WDT cannot be stopped, so while the stub is halted waiting for /
// talking to GDB we must reload it or the board resets out from under the
// debug session after the timeout. RREN enables RR0 only; writing the reload
// magic to RR0 kicks it. Harmless no-op when the WDT isn't running.
constexpr uint32_t WDT_BASE = 0x40010000UL;
constexpr uint32_t WDT_RUNSTATUS = 0x400UL;
constexpr uint32_t WDT_RR0 = 0x600UL;
constexpr uint32_t WDT_RELOAD_MAGIC = 0x6E524635UL;
constexpr uint32_t ARM_FLASH_END = 0x00100000UL;
constexpr uint32_t ARM_SRAM_BASE = 0x20000000UL;
constexpr uint32_t ARM_SRAM_END = 0x20040000UL;
constexpr size_t PACKET_CAPACITY = 256U;
constexpr size_t REPLY_CAPACITY = 384U;
constexpr uint8_t GDB_BREADCRUMB_BASE = 0xA0U;
constexpr uint8_t GDB_BREADCRUMB_EXCEPTION = 0xA1U;
constexpr uint8_t GDB_BREADCRUMB_INIT = 0xA2U;
constexpr uint8_t GDB_BREADCRUMB_CAPTURE = 0xA3U;
constexpr uint8_t GDB_BREADCRUMB_SERVE = 0xA4U;
constexpr uint8_t GDB_BREADCRUMB_STOP = 0xA5U;
constexpr uint8_t GDB_BREADCRUMB_PACKET = 0xA6U;
constexpr uint8_t GDB_BREADCRUMB_REPLY = 0xA7U;
constexpr uint32_t GDB_USB_REENUMERATION_SPINS = 3200000UL;

inline volatile uint32_t &mem32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}

// Reload the hardware watchdog if (and only if) it is running, so a sketch
// that armed the WDT before a breakpoint doesn't reset the board while the
// stub is halted waiting for or servicing GDB.
inline void feedWatchdogIfRunning() {
    if (*reinterpret_cast<volatile uint32_t *>(WDT_BASE + WDT_RUNSTATUS) != 0UL) {
        *reinterpret_cast<volatile uint32_t *>(WDT_BASE + WDT_RR0) = WDT_RELOAD_MAGIC;
    }
}

inline volatile uint32_t &powerReg32(uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(POWER_BASE + offset);
}

bool ownsBreadcrumb(uint8_t value) {
    return (value & 0xF0U) == GDB_BREADCRUMB_BASE;
}

void writeBreadcrumb(uint8_t value) {
    powerReg32(POWER_GPREGRET) = value;
}

uint8_t readBreadcrumb() {
    return static_cast<uint8_t>(powerReg32(POWER_GPREGRET) & 0xFFU);
}

void clearBreadcrumb() {
    if (ownsBreadcrumb(readBreadcrumb())) {
        powerReg32(POWER_GPREGRET) = 0U;
    }
}

inline uint8_t toHex(uint8_t value) {
    if (value < 10U) {
        return static_cast<uint8_t>('0' + value);
    }
    return static_cast<uint8_t>('a' + (value - 10U));
}

int fromHex(int value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

void appendChar(char *buffer, size_t capacity, size_t &length, char value) {
    if (length + 1U >= capacity) {
        return;
    }
    buffer[length++] = value;
    buffer[length] = '\0';
}

void appendText(char *buffer, size_t capacity, size_t &length, const char *text) {
    if (text == nullptr) {
        return;
    }
    while (*text != '\0' && length + 1U < capacity) {
        buffer[length++] = *text++;
    }
    buffer[length] = '\0';
}

void appendByte(char *buffer, size_t capacity, size_t &length, uint8_t value) {
    appendChar(buffer, capacity, length, static_cast<char>(toHex(static_cast<uint8_t>(value >> 4U))));
    appendChar(buffer, capacity, length, static_cast<char>(toHex(static_cast<uint8_t>(value & 0x0FU))));
}

void appendWord(char *buffer, size_t capacity, size_t &length, uint32_t value) {
    for (size_t index = 0; index < 4U; ++index) {
        appendByte(buffer, capacity, length, static_cast<uint8_t>((value >> (index * 8U)) & 0xFFU));
    }
}

uint32_t parseHexWord(const char *text, bool &ok) {
    ok = false;
    if (text == nullptr || *text == '\0') {
        return 0UL;
    }

    uint32_t value = 0UL;
    while (*text != '\0') {
        const int nibble = fromHex(*text++);
        if (nibble < 0) {
            return 0UL;
        }
        value = static_cast<uint32_t>((value << 4U) | static_cast<uint32_t>(nibble));
    }
    ok = true;
    return value;
}

bool splitMemoryRequest(char *packet, uint32_t &address, uint32_t &length) {
    char *comma = nullptr;
    for (char *cursor = packet; *cursor != '\0'; ++cursor) {
        if (*cursor == ',') {
            comma = cursor;
            break;
        }
    }
    if (comma == nullptr) {
        return false;
    }

    *comma = '\0';
    bool okAddress = false;
    bool okLength = false;
    address = parseHexWord(packet, okAddress);
    length = parseHexWord(comma + 1, okLength);
    return okAddress && okLength;
}

bool memoryReadable(uint32_t address, uint32_t length) {
    const uint64_t end = static_cast<uint64_t>(address) + static_cast<uint64_t>(length);
    if (end < address) {
        return false;
    }
    if (address < ARM_FLASH_END && end <= ARM_FLASH_END) {
        return true;
    }
    if (address >= ARM_SRAM_BASE && end <= ARM_SRAM_END) {
        return true;
    }
    return false;
}

bool isFaultException(uint32_t exceptionNumber) {
    return exceptionNumber == 3UL || exceptionNumber == 4UL || exceptionNumber == 5UL || exceptionNumber == 6UL;
}

bool parseRegisterBytes(const char *text, uint32_t &value) {
    if (text == nullptr) {
        return false;
    }

    value = 0UL;
    for (size_t index = 0; index < 4U; ++index) {
        const int high = fromHex(text[index * 2U]);
        const int low = fromHex(text[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return false;
        }
        const uint8_t byteValue = static_cast<uint8_t>((high << 4U) | low);
        value |= static_cast<uint32_t>(byteValue) << (index * 8U);
    }
    return true;
}

bool setRegByIndex(NrfGdbRegs &regs, uint32_t index, uint32_t value) {
    switch (index) {
        case 0UL:
            regs.r0 = value;
            return true;
        case 1UL:
            regs.r1 = value;
            return true;
        case 2UL:
            regs.r2 = value;
            return true;
        case 3UL:
            regs.r3 = value;
            return true;
        case 4UL:
            regs.r4 = value;
            return true;
        case 5UL:
            regs.r5 = value;
            return true;
        case 6UL:
            regs.r6 = value;
            return true;
        case 7UL:
            regs.r7 = value;
            return true;
        case 8UL:
            regs.r8 = value;
            return true;
        case 9UL:
            regs.r9 = value;
            return true;
        case 10UL:
            regs.r10 = value;
            return true;
        case 11UL:
            regs.r11 = value;
            return true;
        case 12UL:
            regs.r12 = value;
            return true;
        case 13UL:
            return value == regs.sp;
        case 14UL:
            regs.lr = value;
            return true;
        case 15UL:
            regs.pc = value;
            return true;
        case 25UL:
            regs.xpsr = value;
            return true;
        default:
            return false;
    }
}

uint32_t thumbInstructionSize(uint32_t pc) {
    const uint32_t address = pc & ~1UL;
    if (!memoryReadable(address, 2UL)) {
        return 2UL;
    }

    const uint16_t opcode = *reinterpret_cast<const uint16_t *>(address);
    if ((opcode & 0xF800U) == 0xE800U || (opcode & 0xF000U) == 0xF000U) {
        return 4UL;
    }
    return 2UL;
}

uint8_t signalForException(uint32_t exceptionNumber) {
    switch (exceptionNumber) {
        case 4UL:
            return 11U;
        case 5UL:
            return 10U;
        case 6UL:
            return 4U;
        case 12UL:
            return 5U;
        case 3UL:
        default:
            return 11U;
    }
}

const uint32_t *regByIndex(const NrfGdbRegs &regs, uint32_t index) {
    switch (index) {
        case 0UL:
            return &regs.r0;
        case 1UL:
            return &regs.r1;
        case 2UL:
            return &regs.r2;
        case 3UL:
            return &regs.r3;
        case 4UL:
            return &regs.r4;
        case 5UL:
            return &regs.r5;
        case 6UL:
            return &regs.r6;
        case 7UL:
            return &regs.r7;
        case 8UL:
            return &regs.r8;
        case 9UL:
            return &regs.r9;
        case 10UL:
            return &regs.r10;
        case 11UL:
            return &regs.r11;
        case 12UL:
            return &regs.r12;
        case 13UL:
            return &regs.sp;
        case 14UL:
            return &regs.lr;
        case 15UL:
            return &regs.pc;
        case 25UL:
            return &regs.xpsr;
        default:
            return nullptr;
    }
}
}

NrfGdbStubClass &nrfGdbStub() {
    static NrfGdbStubClass stub;
    return stub;
}

uint8_t nrfGdbStubBreadcrumb() {
    const uint8_t value = readBreadcrumb();
    if (ownsBreadcrumb(value)) {
        return value;
    }
    return 0U;
}

void nrfGdbStubClearBreadcrumb() {
    clearBreadcrumb();
}

void NrfGdbStubClass::init() {
    if (initialized_ || !enabled()) {
        return;
    }

    nrfUsbdDriver().end();
    for (volatile uint32_t spin = 0UL; spin < GDB_USB_REENUMERATION_SPINS; ++spin) {
        __asm__ volatile("nop");
    }
    USBDevice.init();
    USBDevice.attach();
    // TRCENA must be set before any FPB/DWT register write.
    mem32(DEMCR) |= DEMCR_TRCENA | DEMCR_MON_EN;
    mem32(SHCSR) |= SHCSR_MEMFAULTENA | SHCSR_BUSFAULTENA | SHCSR_USGFAULTENA;
    fpbInitOnce();
    initialized_ = true;
    writeBreadcrumb(GDB_BREADCRUMB_INIT);
}

void NrfGdbStubClass::fpbInitOnce() {
    if (fpbReady_) {
        return;
    }
    // Enable the FPB unit. The KEY bit must be set on every write to FP_CTRL.
    mem32(FP_CTRL) = FP_CTRL_KEY | FP_CTRL_ENABLE;
    for (size_t index = 0; index < kFpbMaxSlots; ++index) {
        mem32(FP_COMP_BASE + (index * 4U)) = 0UL;
        fpbSlots_[index].inUse = false;
        fpbSlots_[index].address = 0UL;
    }
    fpbReady_ = true;
}

bool NrfGdbStubClass::fpbInsert(uint32_t address) {
    fpbInitOnce();
    // FPB only acts on flash (code) addresses. RAM-resident code would need
    // an actual BKPT byte patched in, which we don't support today.
    const uint32_t bare = address & ~1UL;
    if (bare >= ARM_FLASH_END) {
        return false;
    }
    // Reuse an existing slot if this address is already armed.
    for (size_t index = 0; index < kFpbMaxSlots; ++index) {
        if (fpbSlots_[index].inUse && fpbSlots_[index].address == bare) {
            return true;
        }
    }
    for (size_t index = 0; index < kFpbMaxSlots; ++index) {
        if (fpbSlots_[index].inUse) {
            continue;
        }
        const uint32_t replace = (bare & 0x2UL) ? FP_COMP_REPLACE_HI : FP_COMP_REPLACE_LO;
        mem32(FP_COMP_BASE + (index * 4U)) = replace | (bare & FP_COMP_ADDR_MASK) | FP_COMP_ENABLE;
        fpbSlots_[index].inUse = true;
        fpbSlots_[index].address = bare;
        return true;
    }
    return false; // All 6 comparators are in use.
}

bool NrfGdbStubClass::fpbRemove(uint32_t address) {
    const uint32_t bare = address & ~1UL;
    for (size_t index = 0; index < kFpbMaxSlots; ++index) {
        if (fpbSlots_[index].inUse && fpbSlots_[index].address == bare) {
            mem32(FP_COMP_BASE + (index * 4U)) = 0UL;
            fpbSlots_[index].inUse = false;
            fpbSlots_[index].address = 0UL;
            return true;
        }
    }
    return false;
}

void NrfGdbStubClass::fpbDisableAll() {
    for (size_t index = 0; index < kFpbMaxSlots; ++index) {
        mem32(FP_COMP_BASE + (index * 4U)) = 0UL;
        fpbSlots_[index].inUse = false;
        fpbSlots_[index].address = 0UL;
    }
}

void NrfGdbStubClass::enableMonStep() {
    mem32(DEMCR) |= DEMCR_MON_STEP;
}

void NrfGdbStubClass::disableMonStep() {
    mem32(DEMCR) &= ~DEMCR_MON_STEP;
}

bool NrfGdbStubClass::memoryWritable(uint32_t address, uint32_t length) const {
    const uint64_t end = static_cast<uint64_t>(address) + static_cast<uint64_t>(length);
    if (end < address) {
        return false;
    }
    // Only RAM is writable directly. Flash patching would require NVMC erase
    // of a 4 KB page and is intentionally not supported here.
    return address >= ARM_SRAM_BASE && end <= ARM_SRAM_END;
}

bool NrfGdbStubClass::writeMemoryBytes(uint32_t address, const uint8_t *data, uint32_t length) {
    if (!memoryWritable(address, length)) {
        return false;
    }
    uint8_t *destination = reinterpret_cast<uint8_t *>(address);
    for (uint32_t index = 0UL; index < length; ++index) {
        destination[index] = data[index];
    }
    return true;
}

void NrfGdbStubClass::poll() {
    if (!enabled()) {
        return;
    }
    // CRITICAL: pump irqHandler(), not poll(). While the stub is halted it masks
    // the USBD NVIC IRQ, so the hardware ISR can't run. The host->device (OUT)
    // draining - EVENTS_EPDATA -> TASKS_STARTEPOUT -> EVENTS_ENDEPOUT -> rx ring -
    // lives ONLY in irqHandler(); poll() services device->host (IN) but never
    // drains OUT. Calling poll() alone meant GDB's RSP packets were never read
    // off the service CDC: the OUT endpoint stayed full, host writes stalled
    // ("semaphore timeout"), and GDB saw the connection close. Manually pumping
    // irqHandler() (safe precisely because the IRQ is masked) drains OUT so the
    // stub actually receives commands.
    nrfUsbdDriver().irqHandler();
}

bool NrfGdbStubClass::enabled() const {
    const NrfSystemProfile &profile = nrfSystemProfile();
    return nrfUsbServicePortEnabled() && profile.debugTransport == NrfDebugTransport::UsbCdcGdbStub;
}

bool NrfGdbStubClass::active() const {
    return active_;
}

void NrfGdbStubClass::capture(uint32_t *stack, uint32_t excReturn, uint32_t exceptionNumber) {
    writeBreadcrumb(GDB_BREADCRUMB_CAPTURE);
    noAckMode_ = false;
    stackFrame_ = stack;
    regs_.r0 = stack[0];
    regs_.r1 = stack[1];
    regs_.r2 = stack[2];
    regs_.r3 = stack[3];
    regs_.r4 = nrf_gdb_stub_callee_regs[0];
    regs_.r5 = nrf_gdb_stub_callee_regs[1];
    regs_.r6 = nrf_gdb_stub_callee_regs[2];
    regs_.r7 = nrf_gdb_stub_callee_regs[3];
    regs_.r8 = nrf_gdb_stub_callee_regs[4];
    regs_.r9 = nrf_gdb_stub_callee_regs[5];
    regs_.r10 = nrf_gdb_stub_callee_regs[6];
    regs_.r11 = nrf_gdb_stub_callee_regs[7];
    regs_.r12 = stack[4];
    regs_.sp = reinterpret_cast<uint32_t>(stack + 8);
    regs_.lr = stack[5];
    regs_.pc = stack[6];
    regs_.xpsr = stack[7];
    excReturn_ = excReturn;
    exceptionNumber_ = exceptionNumber;
    signal_ = signalForException(exceptionNumber);
}

void NrfGdbStubClass::applyRegs() {
    if (stackFrame_ == nullptr) {
        return;
    }

    stackFrame_[0] = regs_.r0;
    stackFrame_[1] = regs_.r1;
    stackFrame_[2] = regs_.r2;
    stackFrame_[3] = regs_.r3;
    nrf_gdb_stub_callee_regs[0] = regs_.r4;
    nrf_gdb_stub_callee_regs[1] = regs_.r5;
    nrf_gdb_stub_callee_regs[2] = regs_.r6;
    nrf_gdb_stub_callee_regs[3] = regs_.r7;
    nrf_gdb_stub_callee_regs[4] = regs_.r8;
    nrf_gdb_stub_callee_regs[5] = regs_.r9;
    nrf_gdb_stub_callee_regs[6] = regs_.r10;
    nrf_gdb_stub_callee_regs[7] = regs_.r11;
    stackFrame_[4] = regs_.r12;
    stackFrame_[5] = regs_.lr;
    stackFrame_[6] = regs_.pc;
    stackFrame_[7] = regs_.xpsr;
}

void NrfGdbStubClass::clearFaults() {
    mem32(CFSR) = mem32(CFSR);
    mem32(HFSR) = mem32(HFSR);
    mem32(DFSR) = mem32(DFSR);
}

void NrfGdbStubClass::continueFrom(const char *packet) {
    if (packet != nullptr && packet[0] == 'c' && packet[1] != '\0') {
        bool ok = false;
        const uint32_t nextPc = parseHexWord(packet + 1, ok);
        if (ok) {
            regs_.pc = nextPc;
        }
    }

    if (packet != nullptr && packet[0] == 'c' && packet[1] == '\0') {
        const uint32_t bare = regs_.pc & ~1UL;
        const bool isFault = isFaultException(exceptionNumber_);
        // If we entered via a BKPT (DebugMon, exception 12), the PC sits ON
        // the BKPT instruction. Auto-advance past it on a plain `c` so the
        // target doesn't immediately re-trap. BKPT is the 16-bit Thumb
        // encoding 0xBE00..0xBEFF.
        bool isBkpt = false;
        if (memoryReadable(bare, 2UL)) {
            const uint16_t opcode = *reinterpret_cast<const uint16_t *>(bare);
            isBkpt = ((opcode & 0xFF00U) == 0xBE00U);
        }
        if (isFault || (exceptionNumber_ == 12UL && isBkpt)) {
            const uint32_t step = thumbInstructionSize(regs_.pc);
            regs_.pc = ((regs_.pc & ~1UL) + step) | (regs_.pc & 1UL);
        }
    }

    clearFaults();
    applyRegs();
    clearBreadcrumb();
}

void NrfGdbStubClass::sendPacket(const char *payload) {
    uint8_t checksum = 0U;
    while (nrfUsbdDriver().write('$') != 1U) {
        poll();
    }
    for (const char *cursor = payload; *cursor != '\0'; ++cursor) {
        checksum = static_cast<uint8_t>(checksum + static_cast<uint8_t>(*cursor));
        while (nrfUsbdDriver().write(static_cast<uint8_t>(*cursor)) != 1U) {
            poll();
        }
    }
    while (nrfUsbdDriver().write('#') != 1U) {
        poll();
    }
    while (nrfUsbdDriver().write(toHex(static_cast<uint8_t>(checksum >> 4U))) != 1U) {
        poll();
    }
    while (nrfUsbdDriver().write(toHex(static_cast<uint8_t>(checksum & 0x0FU))) != 1U) {
        poll();
    }
    nrfUsbdDriver().flush();
}

void NrfGdbStubClass::sendStop() {
    writeBreadcrumb(GDB_BREADCRUMB_STOP);
    char reply[8] = {0};
    size_t length = 0U;
    appendChar(reply, sizeof(reply), length, 'S');
    appendByte(reply, sizeof(reply), length, signal_);
    sendPacket(reply);
}

bool NrfGdbStubClass::readPacket(char *packet, size_t capacity) {
    while (true) {
        // This is the indefinite idle spin while halted at a breakpoint waiting
        // for the next GDB byte. Keep any sketch-armed watchdog fed here, or it
        // would reset the board before the user even attaches.
        feedWatchdogIfRunning();
        poll();
        const int value = nrfUsbdDriver().read();
        if (value < 0) {
            continue;
        }
        writeBreadcrumb(GDB_BREADCRUMB_PACKET);
        if (value == '+') {
            continue;
        }
        if (value == 0x03) {
            packet[0] = '?';
            packet[1] = '\0';
            return true;
        }
        if (value != '$') {
            continue;
        }

        uint8_t checksum = 0U;
        size_t length = 0U;
        while (true) {
            poll();
            const int byteValue = nrfUsbdDriver().read();
            if (byteValue < 0) {
                continue;
            }
            if (byteValue == '#') {
                break;
            }
            checksum = static_cast<uint8_t>(checksum + static_cast<uint8_t>(byteValue));
            if (length + 1U < capacity) {
                packet[length++] = static_cast<char>(byteValue);
                packet[length] = '\0';
            }
        }

        int high = -1;
        int low = -1;
        while (high < 0) {
            poll();
            high = fromHex(nrfUsbdDriver().read());
        }
        while (low < 0) {
            poll();
            low = fromHex(nrfUsbdDriver().read());
        }

        const uint8_t remoteChecksum = static_cast<uint8_t>((high << 4U) | low);
        if (remoteChecksum != checksum) {
            while (nrfUsbdDriver().write('-') != 1U) {
                poll();
            }
            nrfUsbdDriver().flush();
            continue;
        }

        if (!noAckMode_) {
            while (nrfUsbdDriver().write('+') != 1U) {
                poll();
            }
            nrfUsbdDriver().flush();
        }
        return true;
    }
}

void NrfGdbStubClass::reply(char *packet, char *response, size_t capacity) {
    writeBreadcrumb(GDB_BREADCRUMB_REPLY);
    response[0] = '\0';
    size_t length = 0U;

    if (packet[0] == '?') {
        appendChar(response, capacity, length, 'S');
        appendByte(response, capacity, length, signal_);
        return;
    }

    if (packet[0] == 'q') {
        if (strncmp(packet, "qSupported", 10) == 0) {
            // Larger packet size lets gdb pull register dumps in one round trip;
            // hwbreak+ tells gdb our Z0 is satisfied by hardware (FPB) so it
            // won't fall back to writing BKPT into flash.
            appendText(response, capacity, length,
                       "PacketSize=200;swbreak+;hwbreak+;vContSupported+;QStartNoAckMode+");
            return;
        }
        if (strcmp(packet, "qAttached") == 0) {
            appendText(response, capacity, length, "1");
            return;
        }
        if (strcmp(packet, "qTStatus") == 0) {
            return;
        }
        if (strcmp(packet, "qfThreadInfo") == 0) {
            appendText(response, capacity, length, "m1");
            return;
        }
        if (strcmp(packet, "qsThreadInfo") == 0) {
            appendText(response, capacity, length, "l");
            return;
        }
        return;
    }

    if (packet[0] == 'H') {
        appendText(response, capacity, length, "OK");
        return;
    }

    if (strcmp(packet, "vCont?") == 0) {
        appendText(response, capacity, length, "vCont;c;C;s;S");
        return;
    }

    if (strcmp(packet, "vMustReplyEmpty") == 0) {
        return;
    }

    if (strcmp(packet, "!") == 0) {
        appendText(response, capacity, length, "OK");
        return;
    }

    if (strcmp(packet, "QStartNoAckMode") == 0) {
        appendText(response, capacity, length, "OK");
        noAckMode_ = true;
        return;
    }

    if (packet[0] == 'g') {
        appendWord(response, capacity, length, regs_.r0);
        appendWord(response, capacity, length, regs_.r1);
        appendWord(response, capacity, length, regs_.r2);
        appendWord(response, capacity, length, regs_.r3);
        appendWord(response, capacity, length, regs_.r4);
        appendWord(response, capacity, length, regs_.r5);
        appendWord(response, capacity, length, regs_.r6);
        appendWord(response, capacity, length, regs_.r7);
        appendWord(response, capacity, length, regs_.r8);
        appendWord(response, capacity, length, regs_.r9);
        appendWord(response, capacity, length, regs_.r10);
        appendWord(response, capacity, length, regs_.r11);
        appendWord(response, capacity, length, regs_.r12);
        appendWord(response, capacity, length, regs_.sp);
        appendWord(response, capacity, length, regs_.lr);
        appendWord(response, capacity, length, regs_.pc);
        appendWord(response, capacity, length, regs_.xpsr);
        return;
    }

    if (packet[0] == 'p') {
        char *equals = strchr(packet + 1, '=');
        if (equals != nullptr) {
            *equals = '\0';
            bool ok = false;
            const uint32_t index = parseHexWord(packet + 1, ok);
            uint32_t value = 0UL;
            if (!ok || !parseRegisterBytes(equals + 1, value) || !setRegByIndex(regs_, index, value)) {
                appendText(response, capacity, length, "E01");
                return;
            }
            appendText(response, capacity, length, "OK");
            return;
        }

        bool ok = false;
        const uint32_t index = parseHexWord(packet + 1, ok);
        const uint32_t *reg = nullptr;
        if (ok) {
            reg = regByIndex(regs_, index);
        }
        if (reg == nullptr) {
            appendText(response, capacity, length, "E01");
            return;
        }
        appendWord(response, capacity, length, *reg);
        return;
    }

    if (packet[0] == 'G') {
        const char *cursor = packet + 1;
        uint32_t value = 0UL;
        for (uint32_t index = 0UL; index <= 15UL; ++index) {
            if (!parseRegisterBytes(cursor, value) || !setRegByIndex(regs_, index, value)) {
                appendText(response, capacity, length, "E01");
                return;
            }
            cursor += 8;
        }
        if (!parseRegisterBytes(cursor, value) || !setRegByIndex(regs_, 25UL, value)) {
            appendText(response, capacity, length, "E01");
            return;
        }
        appendText(response, capacity, length, "OK");
        return;
    }

    if (packet[0] == 'm') {
        uint32_t address = 0UL;
        uint32_t requestLength = 0UL;
        if (!splitMemoryRequest(packet + 1, address, requestLength) || requestLength > 96UL || !memoryReadable(address, requestLength)) {
            appendText(response, capacity, length, "E01");
            return;
        }

        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(address);
        for (uint32_t index = 0UL; index < requestLength; ++index) {
            appendByte(response, capacity, length, bytes[index]);
        }
        return;
    }

    // M addr,length:bytes — write `length` bytes (hex-encoded) to RAM at `addr`.
    // Refused for flash because we don't perform NVMC operations from the stub.
    if (packet[0] == 'M') {
        char *colon = strchr(packet + 1, ':');
        if (colon == nullptr) {
            appendText(response, capacity, length, "E01");
            return;
        }
        *colon = '\0';
        uint32_t address = 0UL;
        uint32_t writeLength = 0UL;
        if (!splitMemoryRequest(packet + 1, address, writeLength)) {
            appendText(response, capacity, length, "E01");
            return;
        }
        if (writeLength == 0UL) {
            appendText(response, capacity, length, "OK");
            return;
        }

        const char *cursor = colon + 1;
        // Decode in place into a stack buffer; cap matches our packet size.
        uint8_t bytes[96];
        if (writeLength > sizeof(bytes)) {
            appendText(response, capacity, length, "E02");
            return;
        }
        for (uint32_t index = 0UL; index < writeLength; ++index) {
            const int high = fromHex(cursor[index * 2U]);
            const int low = fromHex(cursor[index * 2U + 1U]);
            if (high < 0 || low < 0) {
                appendText(response, capacity, length, "E01");
                return;
            }
            bytes[index] = static_cast<uint8_t>((high << 4U) | low);
        }
        if (!writeMemoryBytes(address, bytes, writeLength)) {
            appendText(response, capacity, length, "E03");
            return;
        }
        appendText(response, capacity, length, "OK");
        return;
    }

    // Z0,addr,kind / z0,addr,kind — software breakpoint insert / remove.
    // We satisfy them with hardware FPB comparators (max 6 simultaneous bps).
    // Z1/z1 are aliased to the same handler since FPB is the only mechanism.
    if (packet[0] == 'Z' || packet[0] == 'z') {
        const bool insert = (packet[0] == 'Z');
        const char type = packet[1];
        if ((type != '0' && type != '1') || packet[2] != ',') {
            return; // Unsupported breakpoint type → empty reply lets gdb retry.
        }
        char *afterAddr = strchr(packet + 3, ',');
        if (afterAddr == nullptr) {
            appendText(response, capacity, length, "E01");
            return;
        }
        *afterAddr = '\0';
        bool ok = false;
        const uint32_t address = parseHexWord(packet + 3, ok);
        if (!ok) {
            appendText(response, capacity, length, "E01");
            return;
        }
        const bool result = insert ? fpbInsert(address) : fpbRemove(address);
        appendText(response, capacity, length, result ? "OK" : "E02");
        return;
    }

    if (packet[0] == 'D') {
        // GDB detach: drop all breakpoints so the program runs cleanly afterwards.
        fpbDisableAll();
        disableMonStep();
        appendText(response, capacity, length, "OK");
        return;
    }
}

void NrfGdbStubClass::serve() {
    writeBreadcrumb(GDB_BREADCRUMB_SERVE);
    nrfUsbdResetPollTrace();
    char packet[PACKET_CAPACITY] = {0};
    char response[REPLY_CAPACITY] = {0};

    while (!USBDevice.connected()) {
        feedWatchdogIfRunning();
        poll();
    }

    while (true) {
        // Halted at a breakpoint or between GDB packets: keep any sketch-armed
        // watchdog alive so the debug session isn't reset out from under us.
        feedWatchdogIfRunning();

        if (stopPending_) {
            sendStop();
            stopPending_ = false;
        }

        if (!readPacket(packet, sizeof(packet))) {
            continue;
        }

        // Continue without stepping: c, c<addr>, vCont;c
        if (packet[0] == 'c' || strncmp(packet, "vCont;c", 7) == 0) {
            disableMonStep();
            continueFrom(packet);
            active_ = false;
            return;
        }

        // Single step: s, s<addr>, vCont;s, vCont;S<sig>
        // Arming MON_STEP causes DebugMon to re-fire after exactly one
        // instruction once we return from this exception.
        if (packet[0] == 's' || strncmp(packet, "vCont;s", 7) == 0 ||
            strncmp(packet, "vCont;S", 7) == 0) {
            enableMonStep();
            // Pass plain 'c' so continueFrom does the register write-back
            // without trying to skip a faulting instruction.
            char nudge[2] = {'c', '\0'};
            continueFrom(nudge);
            active_ = false;
            return;
        }

        reply(packet, response, sizeof(response));
        sendPacket(response);

        if (packet[0] == 'D') {
            clearFaults();
            active_ = false;
            nrfUsbdClearPollTrace();
            clearBreadcrumb();
            return;
        }
    }
}

void NrfGdbStubClass::handleException(uint32_t *stack, uint32_t excReturn, uint32_t exceptionNumber) {
    if (!enabled()) {
        while (true) {
        }
    }

    writeBreadcrumb(GDB_BREADCRUMB_EXCEPTION);
    init();
    // Mask USBD in NVIC for the duration of the stub session. The peripheral
    // events still latch in EVENTS_*; our explicit poll() services them, so
    // this prevents the ISR from racing the busy-loop reads/writes below.
    usbIrqWasEnabled_ = (mem32(NVIC_ISER_BASE + USBD_NVIC_REG_OFFSET) & USBD_NVIC_BIT) != 0UL;
    if (usbIrqWasEnabled_) {
        mem32(NVIC_ICER_BASE + USBD_NVIC_REG_OFFSET) = USBD_NVIC_BIT;
    }
    capture(stack, excReturn, exceptionNumber);
    active_ = true;
    stopPending_ = true;
    serve();
    if (usbIrqWasEnabled_) {
        mem32(NVIC_ISER_BASE + USBD_NVIC_REG_OFFSET) = USBD_NVIC_BIT;
    }
}

extern "C" void nrfGdbStubHandleExceptionC(uint32_t *stack, uint32_t excReturn, uint32_t exceptionNumber) {
    nrfGdbStub().handleException(stack, excReturn, exceptionNumber);
}

#if defined(NRF_SYSTEM_USB_GDB_STUB) && (NRF_SYSTEM_USB_GDB_STUB == 1)
extern "C" __attribute__((naked)) void HardFault_Handler(void) {
    __asm__ volatile(
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "stmia r3!, {r4-r7}\n"
        "mov r4, r8\n"
        "mov r5, r9\n"
        "mov r6, r10\n"
        "mov r7, r11\n"
        "stmia r3!, {r4-r7}\n"
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r3, lr\n"
        "push {r3, r4}\n"
        "mov r1, lr\n"
        "movs r2, #3\n"
        "bl nrfGdbStubHandleExceptionC\n"
        "pop {r4, lr}\n"
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "ldmia r3!, {r4-r7}\n"
        "ldr r0, [r3]\n"
        "ldr r1, [r3, #4]\n"
        "ldr r2, [r3, #8]\n"
        "ldr r3, [r3, #12]\n"
        "mov r8, r0\n"
        "mov r9, r1\n"
        "mov r10, r2\n"
        "mov r11, r3\n"
        "bx lr\n");
}

extern "C" __attribute__((naked)) void MemManage_Handler(void) {
    __asm__ volatile(
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "stmia r3!, {r4-r7}\n"
        "mov r4, r8\n"
        "mov r5, r9\n"
        "mov r6, r10\n"
        "mov r7, r11\n"
        "stmia r3!, {r4-r7}\n"
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r3, lr\n"
        "push {r3, r4}\n"
        "mov r1, lr\n"
        "movs r2, #4\n"
        "bl nrfGdbStubHandleExceptionC\n"
        "pop {r4, lr}\n"
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "ldmia r3!, {r4-r7}\n"
        "ldr r0, [r3]\n"
        "ldr r1, [r3, #4]\n"
        "ldr r2, [r3, #8]\n"
        "ldr r3, [r3, #12]\n"
        "mov r8, r0\n"
        "mov r9, r1\n"
        "mov r10, r2\n"
        "mov r11, r3\n"
        "bx lr\n");
}

extern "C" __attribute__((naked)) void BusFault_Handler(void) {
    __asm__ volatile(
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "stmia r3!, {r4-r7}\n"
        "mov r4, r8\n"
        "mov r5, r9\n"
        "mov r6, r10\n"
        "mov r7, r11\n"
        "stmia r3!, {r4-r7}\n"
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r3, lr\n"
        "push {r3, r4}\n"
        "mov r1, lr\n"
        "movs r2, #5\n"
        "bl nrfGdbStubHandleExceptionC\n"
        "pop {r4, lr}\n"
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "ldmia r3!, {r4-r7}\n"
        "ldr r0, [r3]\n"
        "ldr r1, [r3, #4]\n"
        "ldr r2, [r3, #8]\n"
        "ldr r3, [r3, #12]\n"
        "mov r8, r0\n"
        "mov r9, r1\n"
        "mov r10, r2\n"
        "mov r11, r3\n"
        "bx lr\n");
}

extern "C" __attribute__((naked)) void UsageFault_Handler(void) {
    __asm__ volatile(
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "stmia r3!, {r4-r7}\n"
        "mov r4, r8\n"
        "mov r5, r9\n"
        "mov r6, r10\n"
        "mov r7, r11\n"
        "stmia r3!, {r4-r7}\n"
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r3, lr\n"
        "push {r3, r4}\n"
        "mov r1, lr\n"
        "movs r2, #6\n"
        "bl nrfGdbStubHandleExceptionC\n"
        "pop {r4, lr}\n"
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "ldmia r3!, {r4-r7}\n"
        "ldr r0, [r3]\n"
        "ldr r1, [r3, #4]\n"
        "ldr r2, [r3, #8]\n"
        "ldr r3, [r3, #12]\n"
        "mov r8, r0\n"
        "mov r9, r1\n"
        "mov r10, r2\n"
        "mov r11, r3\n"
        "bx lr\n");
}

extern "C" __attribute__((naked)) void DebugMon_Handler(void) {
    __asm__ volatile(
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "stmia r3!, {r4-r7}\n"
        "mov r4, r8\n"
        "mov r5, r9\n"
        "mov r6, r10\n"
        "mov r7, r11\n"
        "stmia r3!, {r4-r7}\n"
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r3, lr\n"
        "push {r3, r4}\n"
        "mov r1, lr\n"
        "movs r2, #12\n"
        "bl nrfGdbStubHandleExceptionC\n"
        "pop {r4, lr}\n"
        "ldr r3, =nrf_gdb_stub_callee_regs\n"
        "ldmia r3!, {r4-r7}\n"
        "ldr r0, [r3]\n"
        "ldr r1, [r3, #4]\n"
        "ldr r2, [r3, #8]\n"
        "ldr r3, [r3, #12]\n"
        "mov r8, r0\n"
        "mov r9, r1\n"
        "mov r10, r2\n"
        "mov r11, r3\n"
        "bx lr\n");
}
#endif
