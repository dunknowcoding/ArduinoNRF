#include "NrfUsbd.h"
#include "TaichiUsb.h"  // TaichiUSB identity + build-flag guard (see header)

#include <cstring>

#include "Arduino.h"
#include "NrfBoard.h"
#include "NrfSystem.h"
#include "PluggableUSB.h"

namespace {
constexpr uint8_t USB_DIR_OUT = 0x00U;
constexpr uint8_t USB_DIR_IN = 0x80U;
constexpr uint8_t USB_REQ_GET_STATUS = 0x00U;
constexpr uint8_t USB_REQ_SET_ADDRESS = 0x05U;
constexpr uint8_t USB_REQ_GET_DESCRIPTOR = 0x06U;
constexpr uint8_t USB_REQ_GET_CONFIGURATION = 0x08U;
constexpr uint8_t USB_REQ_SET_CONFIGURATION = 0x09U;
constexpr uint8_t USB_REQ_GET_INTERFACE = 0x0AU;
constexpr uint8_t USB_REQ_SET_INTERFACE = 0x0BU;
constexpr uint8_t USB_REQ_CLEAR_FEATURE = 0x01U;
constexpr uint8_t USB_REQ_SET_FEATURE = 0x03U;
constexpr uint8_t USB_REQ_TYPE_STANDARD = 0x00U;
constexpr uint8_t USB_REQ_TYPE_CLASS = 0x20U;
constexpr uint8_t USB_REQ_RECIPIENT_DEVICE = 0x00U;
constexpr uint8_t USB_REQ_RECIPIENT_INTERFACE = 0x01U;
constexpr uint8_t USB_REQ_RECIPIENT_ENDPOINT = 0x02U;
constexpr uint8_t USB_DESC_DEVICE = 0x01U;
constexpr uint8_t USB_DESC_CONFIGURATION = 0x02U;
constexpr uint8_t USB_DESC_STRING = 0x03U;
constexpr uint8_t USB_DESC_INTERFACE = 0x04U;
constexpr uint8_t USB_DESC_ENDPOINT = 0x05U;
constexpr uint8_t USB_DESC_IAD = 0x0BU;
constexpr uint8_t USB_DESC_CS_INTERFACE = 0x24U;
constexpr uint8_t USB_DESC_DFU_FUNCTIONAL = 0x21U;
constexpr uint8_t CDC_REQ_SET_LINE_CODING = 0x20U;
constexpr uint8_t CDC_REQ_GET_LINE_CODING = 0x21U;
constexpr uint8_t CDC_REQ_SET_CONTROL_LINE_STATE = 0x22U;
constexpr uint8_t CDC_NOTIFICATION_SERIAL_STATE = 0x20U;
constexpr uint8_t DFU_REQ_DETACH = 0x00U;
constexpr uint8_t DFU_REQ_GETSTATUS = 0x03U;
constexpr uint8_t DFU_REQ_GETSTATE = 0x05U;
constexpr uint8_t DFU_STATE_APP_IDLE = 0x00U;
constexpr uint8_t DFU_STATUS_OK = 0x00U;
constexpr uint32_t USBD_BASE = 0x40027000UL;
constexpr uint32_t POWER_BASE = 0x40000000UL;
constexpr uint32_t CLOCK_BASE = 0x40000000UL;
constexpr uint32_t TASKS_STARTEPIN_BASE = 0x004UL;
constexpr uint32_t TASKS_STARTEPOUT_BASE = 0x028UL;
constexpr uint32_t TASKS_EP0RCVOUT = 0x04CUL;
constexpr uint32_t TASKS_EP0STATUS = 0x050UL;
constexpr uint32_t TASKS_EP0STALL = 0x054UL;
constexpr uint32_t TASKS_DPDMNODRIVE = 0x05CUL;
constexpr uint32_t EVENTS_USBRESET = 0x100UL;
constexpr uint32_t EVENTS_STARTED = 0x104UL;
constexpr uint32_t EVENTS_ENDEPIN_BASE = 0x108UL;
constexpr uint32_t EVENTS_EP0DATADONE = 0x128UL;
constexpr uint32_t EVENTS_ENDISOIN = 0x12CUL;
constexpr uint32_t EVENTS_ENDEPOUT_BASE = 0x130UL;
constexpr uint32_t EVENTS_ENDISOOUT = 0x150UL;
constexpr uint32_t EVENTS_SOF = 0x154UL;
constexpr uint32_t EVENTS_USBEVENT = 0x158UL;
constexpr uint32_t EVENTS_EP0SETUP = 0x15CUL;
constexpr uint32_t EVENTS_EPDATA = 0x160UL;
// nRF52840 USBD register map: EPSTATUS=0x468 (which endpoints' EasyDMA was
// captured), EPDATASTATUS=0x46C (which endpoints had an acknowledged host data
// transfer -- the "this OUT packet is buffered, drain it" signal). USBADDR at
// 0x470 is read-only and is intentionally not exposed as a writable constant.
// This was previously 0x468, i.e. the firmware read EPSTATUS by mistake: its
// EP0 bits (EPIN0=bit0, EPOUT0=bit16) read as a constant 0x00010001 and its
// EPOUT data bits never reflect a freshly-arrived host packet, so the EPDATA
// handler never drained the service-CDC OUT endpoint — every host write NAK'd
// once the internal buffer filled. (Verified over SWD: with a packet buffered,
// 0x468 reads 0 while 0x46C has EPOUT2/bit18 set.) IN and enumeration were
// unaffected because they key off ENDEPIN/EP0 events, not EPDATASTATUS, which
// is why the GDB stub — the first feature to receive bulk OUT on the service
// CDC — was the first to expose this.
constexpr uint32_t EPSTATUS = 0x468UL;
constexpr uint32_t EPDATASTATUS = 0x46CUL;
constexpr uint32_t SHORTS = 0x200UL;
constexpr uint32_t INTENSET = 0x304UL;
constexpr uint32_t INTENCLR = 0x308UL;
constexpr uint32_t EVENTCAUSE = 0x400UL;
constexpr uint32_t BMREQUESTTYPE = 0x480UL;
constexpr uint32_t BREQUEST = 0x484UL;
constexpr uint32_t WVALUEL = 0x488UL;
constexpr uint32_t WVALUEH = 0x48CUL;
constexpr uint32_t WINDEXL = 0x490UL;
constexpr uint32_t WINDEXH = 0x494UL;
constexpr uint32_t WLENGTHL = 0x498UL;
constexpr uint32_t WLENGTHH = 0x49CUL;
constexpr uint32_t ENABLE = 0x500UL;
constexpr uint32_t USBPULLUP = 0x504UL;
constexpr uint32_t DPDMVALUE = 0x508UL;
constexpr uint32_t DTOGGLE = 0x50CUL;
constexpr uint32_t EPINEN = 0x510UL;
constexpr uint32_t EPOUTEN = 0x514UL;
constexpr uint32_t EPSTALL = 0x518UL;
constexpr uint32_t ISOSPLIT = 0x51CUL;
constexpr uint32_t LOWPOWER = 0x52CUL;
constexpr uint32_t ISOINCONFIG = 0x530UL;
constexpr uint32_t EPOUT_PTR_BASE = 0x700UL;
constexpr uint32_t EPOUT_MAXCNT_BASE = 0x704UL;
constexpr uint32_t EPOUT_AMOUNT_BASE = 0x708UL;
constexpr uint32_t EPIN_PTR_BASE = 0x600UL;
constexpr uint32_t EPIN_MAXCNT_BASE = 0x604UL;
constexpr uint32_t EPIN_AMOUNT_BASE = 0x608UL;
constexpr uint32_t USBD_ENDPOINT_CLUSTER_STRIDE = 0x14UL;
constexpr uint32_t POWER_USBREGSTATUS = 0x438UL;
constexpr uint32_t POWER_USBREGSTATUS_VBUSDETECT = 1UL;
constexpr uint32_t POWER_GPREGRET = 0x51CUL;
constexpr uint32_t NVIC_ISER_BASE = 0xE000E100UL;
constexpr uint32_t NVIC_ICER_BASE = 0xE000E180UL;
constexpr uint32_t NVIC_ISPR_BASE = 0xE000E200UL;
constexpr uint32_t NVIC_ICPR_BASE = 0xE000E280UL;
constexpr uint32_t CLOCK_TASKS_HFCLKSTART = 0x000UL;
constexpr uint32_t CLOCK_EVENTS_HFCLKSTARTED = 0x100UL;
constexpr uint32_t AIRCR = 0xE000ED0CUL;
constexpr uint32_t AIRCR_RESET_KEY = 0x05FA0000UL;
constexpr uint32_t AIRCR_SYSRESETREQ = 0x4UL;
constexpr uint32_t USBD_DIAG_CAUSE_ADDR = 0x20004004UL;
constexpr uint32_t USBD_DIAG_CAUSE_CONFIG_TIMEOUT = 0xCA5E0001UL;
constexpr uint32_t USBD_DIAG_CAUSE_POLL_DETACH = 0xCA5E0002UL;
constexpr uint32_t USBD_DIAG_CAUSE_IRQ_DETACH = 0xCA5E0003UL;
constexpr uint32_t USBD_DIAG_CAUSE_1200_TOUCH = 0xCA5E1200UL;
constexpr uint32_t USBD_DIAG_CAUSE_1200_TOUCH_PENDING = 0xCA5E1201UL;
constexpr uint32_t USBD_DIAG_CAUSE_1200_LINE_CODING = 0xCA5E1202UL;
constexpr uint32_t USBD_DIAG_CAUSE_DFU_DETACH = 0xCA5E0DF0UL;
constexpr uint32_t USBD_DIAG_CAUSE_SERVICE_DATA_RX = 0xCA5E7200UL;
constexpr uint32_t USBD_DIAG_CAUSE_DIAG_STAGE_BASE = 0xCA5ED000UL;
constexpr uint32_t USBD_DETACH_REQUEST_MAGIC = 0xD37ACAFEUL;
constexpr uint32_t USBD_ENABLE_VALUE = 1UL;
constexpr uint32_t USBD_IRQ_NUMBER = 39UL;
constexpr uint32_t USBD_INT_USBRESET_MASK = (1UL << 0);
constexpr uint32_t USBD_INT_STARTED_MASK = (1UL << 1);
constexpr uint32_t USBD_INT_ENDEPIN0_MASK = (1UL << 2);
constexpr uint32_t USBD_INT_ENDEPIN1_MASK = (1UL << 3);
constexpr uint32_t USBD_INT_ENDEPIN2_MASK = (1UL << 4);
constexpr uint32_t USBD_INT_ENDEPIN3_MASK = (1UL << 5);
constexpr uint32_t USBD_INT_ENDEPIN4_MASK = (1UL << 6);
constexpr uint32_t USBD_INT_ENDEPIN5_MASK = (1UL << 7);
constexpr uint32_t USBD_INT_ENDEPIN6_MASK = (1UL << 8);
constexpr uint32_t USBD_INT_ENDEPIN7_MASK = (1UL << 9);
constexpr uint32_t USBD_INT_EP0DATADONE_MASK = (1UL << 10);
constexpr uint32_t USBD_INT_ENDEPOUT2_MASK = (1UL << 14);  // nRF52840 PS: ENDEPOUT[2] = bit 14
constexpr uint32_t USBD_INT_ENDEPOUT4_MASK = (1UL << 16);  // nRF52840 PS: ENDEPOUT[4] = bit 16
constexpr uint32_t USBD_INT_USBEVENT_MASK = (1UL << 22);
constexpr uint32_t USBD_INT_EP0SETUP_MASK = (1UL << 23);
// EPDATA fires whenever a non-EP0 IN/OUT transaction completes on the wire.
// EPDATASTATUS bits 17..23 indicate which OUT endpoint received data and
// needs an EasyDMA START to copy it from the internal buffer to RAM
// (Nordic: USBD_EPDATASTATUS_EPOUT1_Pos=17, EPOUT2=18 ... EPOUT7=23). The
// per-endpoint bit is therefore (17 + endpoint - 1) == (16 + endpoint).
// NOTE: this base was 16 (giving bit 17 for EP2 instead of 18), so the
// service-CDC OUT data-ready bit never matched, queueDataOut() was never
// called, and the first host OUT packet sat undrained in the endpoint's
// internal buffer — wedging every subsequent OUT into a NAK. That was the
// real cause of the GDB-stub "service CDC OUT NAKs while halted" bug: the
// stub is simply the first feature to receive bulk OUT on the service CDC
// (uploads only use EP0 control), so the latent off-by-one finally bit.
constexpr uint32_t USBD_INT_EPDATA_MASK = (1UL << 24);
constexpr uint32_t EPSTATUS_OUT_BASE_BIT = 16U;
constexpr uint32_t EPDATASTATUS_OUT_BASE_BIT = 17U;
constexpr uint32_t USBD_EVENTCAUSE_SUSPEND_MASK = (1UL << 8);
constexpr uint32_t USBD_EVENTCAUSE_RESUME_MASK = (1UL << 9);
constexpr uint32_t USBD_EVENTCAUSE_USBWUALLOWED_MASK = (1UL << 10);
constexpr uint32_t USBD_EVENTCAUSE_READY_MASK = (1UL << 11);
constexpr uint8_t SERVICE_NOTIFICATION_EP = 1U;
constexpr uint8_t SERVICE_DATA_EP = 2U;
constexpr uint8_t USER_NOTIFICATION_EP = 3U;
constexpr uint8_t USER_DATA_EP = 4U;
constexpr uint8_t SERVICE_CONTROL_INTERFACE = 0U;
constexpr uint8_t SERVICE_DATA_INTERFACE = 1U;
constexpr uint8_t USER_CONTROL_INTERFACE = 2U;
constexpr uint8_t USER_DATA_INTERFACE = 3U;
constexpr uint8_t USB_STRING_MANUFACTURER = 1U;
constexpr uint8_t USB_STRING_PRODUCT = 2U;
constexpr uint8_t USB_STRING_SERIAL = 3U;
constexpr uint8_t USB_STRING_SERVICE_CONTROL = 4U;
constexpr uint8_t USB_STRING_SERVICE_DATA = 5U;
constexpr uint8_t USB_STRING_USER_CONTROL = 6U;
constexpr uint8_t USB_STRING_USER_DATA = 7U;
constexpr uint8_t USB_STRING_DFU = 8U;
constexpr size_t CONTROL_EP_MAX_PACKET = 64U;
constexpr size_t DATA_EP_MAX_PACKET = 64U;
constexpr uint16_t CDC_SERIAL_STATE_DCD = 0x0001U;
constexpr uint16_t CDC_SERIAL_STATE_DSR = 0x0002U;
constexpr uint8_t USB_MAX_ENDPOINTS = 8U;
constexpr uint32_t USBD_TRACE_MAGIC = 0x55444254UL;
constexpr uint32_t USBD_DTOGGLE_VALUE_POS = 8UL;
constexpr uint32_t USBD_DTOGGLE_NOP = 0UL;
constexpr uint32_t USBD_DTOGGLE_DATA0 = 1UL;
constexpr uint32_t USBD_START_CAPTURE_TIMEOUT_SPINS = 100000UL;
constexpr uint32_t USBD_DISABLE_TIMEOUT_SPINS = 64000UL;
constexpr uint32_t USBD_PARITY_REPAIR_TIMEOUT_SPINS = 64000UL;
constexpr uint32_t USBD_HANDOFF_DETACH_MS = 20UL;
constexpr uint32_t USBD_ISOSPLIT_HALF_VALUE = 0x0080UL;
constexpr uint32_t USBD_ERRATA166_INDEX_ADDRESS = USBD_BASE + 0x800UL;
constexpr uint32_t USBD_ERRATA166_VALUE_ADDRESS = USBD_BASE + 0x804UL;
constexpr uint32_t USBD_ERRATA199_DMA_PENDING_ADDRESS = 0x40027C1CUL;
constexpr uint32_t USBD_ERRATA199_DMA_PENDING_VALUE = 0x00000082UL;
// Bounded spin for flush() while the GDB stub is halted (ISR-mode). Generous —
// the USBD ISR drains the IN ring between kicks — but finite so a host that
// stops reading can't wedge the stub forever.
constexpr uint32_t USBD_STUB_FLUSH_SPINS = 2000000UL;
// Bounded wait for the speculative OUT drain's STARTEPOUT to complete (ENDEPOUT)
// each stub poll. Short — the EasyDMA copy is a few cycles when data is present,
// and on an empty endpoint ENDEPOUT still fires with AMOUNT=0.
constexpr uint32_t USBD_DRAIN_OUT_SPINS = 4000UL;
constexpr size_t USBD_RING_BUFFER_SIZE = 256U;
#if defined(NRF_USBD_CONFIG_TIMEOUT_RESET_MS)
constexpr uint32_t USBD_CONFIG_TIMEOUT_RESET_MS = static_cast<uint32_t>(NRF_USBD_CONFIG_TIMEOUT_RESET_MS);
#else
constexpr uint32_t USBD_CONFIG_TIMEOUT_RESET_MS = 0UL;
#endif
#if defined(NRF_USBD_1200_RESET_ARM_MS)
constexpr uint32_t USBD_1200_RESET_ARM_MS = static_cast<uint32_t>(NRF_USBD_1200_RESET_ARM_MS);
#else
constexpr uint32_t USBD_1200_RESET_ARM_MS = 3000UL;
#endif
#if defined(NRF_USBD_IGNORE_INITIAL_1200_RESET_COUNT)
constexpr uint8_t USBD_IGNORE_INITIAL_1200_RESET_COUNT = static_cast<uint8_t>(NRF_USBD_IGNORE_INITIAL_1200_RESET_COUNT);
#else
constexpr uint8_t USBD_IGNORE_INITIAL_1200_RESET_COUNT = 0U;
#endif
constexpr uint32_t USBD_TOUCH_RESET_CONFIRM_MS = 40UL;
constexpr uint32_t USBD_STARTUP_RETRY_MS = 25UL;

// Compile-time register-map guards. These are intentionally expressed in the
// core rather than a host-only test so every supported Arduino build checks
// the two EasyDMA OUT interrupt positions that previously drifted by one bit.
static_assert(USBD_INT_ENDEPOUT2_MASK == (1UL << 14));
static_assert(USBD_INT_ENDEPOUT4_MASK == (1UL << 16));
static_assert((USBD_INT_ENDEPIN5_MASK | USBD_INT_ENDEPIN6_MASK |
               USBD_INT_ENDEPIN7_MASK) == (0x7UL << 7));
static_assert((1UL << (EPSTATUS_OUT_BASE_BIT + 0U)) == (1UL << 16));
static_assert(TASKS_DPDMNODRIVE == 0x05CUL);
static_assert(EVENTS_ENDISOIN == 0x12CUL);
static_assert(EVENTS_ENDISOOUT == 0x150UL);
static_assert(LOWPOWER == 0x52CUL);
static_assert(USBD_EVENTCAUSE_USBWUALLOWED_MASK == (1UL << 10));
static_assert(USBD_ISOSPLIT_HALF_VALUE == 0x0080UL);
static_assert(USBD_ERRATA166_INDEX_ADDRESS == 0x40027800UL);
static_assert(USBD_ERRATA166_VALUE_ADDRESS == 0x40027804UL);

constexpr bool dmaParityAfterAmount(bool wasOdd, uint32_t amount) {
    return wasOdd != ((amount & 1UL) != 0UL);
}

constexpr bool dmaDisableMayCommit(bool wakeAcknowledged,
                                   bool dmaSettled,
                                   bool parityEven) {
    return wakeAcknowledged && dmaSettled && parityEven;
}

// Nordic's current nrfx_usbd tracks cumulative completed EasyDMA bytes. A
// zero/even transfer preserves parity; every odd AMOUNT toggles it.
static_assert(!dmaParityAfterAmount(false, 0UL));
static_assert(dmaParityAfterAmount(false, 1UL));
static_assert(!dmaParityAfterAmount(true, 1UL));
static_assert(dmaParityAfterAmount(true, 64UL));
static_assert(dmaDisableMayCommit(true, true, true));
static_assert(!dmaDisableMayCommit(false, true, true));
static_assert(!dmaDisableMayCommit(true, false, true));
static_assert(!dmaDisableMayCommit(true, true, false));

constexpr bool controlInNeedsZlp(size_t actualLength,
                                 size_t requestedLength,
                                 size_t maxPacket = CONTROL_EP_MAX_PACKET) {
    return actualLength > 0U && actualLength < requestedLength &&
           maxPacket > 0U && (actualLength % maxPacket) == 0U;
}

// USB 2.0 control-read termination truth table: a full-size final packet only
// needs an extra ZLP when the device returned fewer bytes than the host asked
// for. If actual == wLength, the host already knows the transfer is complete.
static_assert(controlInNeedsZlp(64U, 65U));
static_assert(controlInNeedsZlp(128U, 255U));
static_assert(!controlInNeedsZlp(64U, 64U));
static_assert(!controlInNeedsZlp(63U, 64U));
static_assert(!controlInNeedsZlp(0U, 64U));

inline bool servicePortEnabled() {
    return nrfUsbServicePortEnabled();
}

inline bool userPortEnabled() {
    return nrfUsbUserPortEnabled();
}

inline uint8_t dfuInterfaceNumber() {
    return userPortEnabled() ? 4U : 2U;
}

inline uint8_t firstDynamicEndpoint() {
    return userPortEnabled() ? 5U : 3U;
}

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline volatile uint32_t &mem32(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t *>(address);
}

inline void enableNvicIrq(uint32_t irqNumber) {
    reg32(NVIC_ISER_BASE, (irqNumber / 32UL) * 4UL) = 1UL << (irqNumber % 32UL);
}

inline void disableNvicIrq(uint32_t irqNumber) {
    reg32(NVIC_ICER_BASE, (irqNumber / 32UL) * 4UL) = 1UL << (irqNumber % 32UL);
}

inline void clearPendingNvicIrq(uint32_t irqNumber) {
    reg32(NVIC_ICPR_BASE, (irqNumber / 32UL) * 4UL) = 1UL << (irqNumber % 32UL);
}

inline void setPendingNvicIrq(uint32_t irqNumber) {
    reg32(NVIC_ISPR_BASE, (irqNumber / 32UL) * 4UL) = 1UL << (irqNumber % 32UL);
}

inline bool nvicIrqEnabled(uint32_t irqNumber) {
    return (reg32(NVIC_ISER_BASE, (irqNumber / 32UL) * 4UL) & (1UL << (irqNumber % 32UL))) != 0UL;
}

inline void keepOnlyUsbdWakeInterrupt() {
#if defined(NRF_USBD_POLL_ONLY) && (NRF_USBD_POLL_ONLY == 1)
    return;
#else
    // Once LOWPOWER disconnects the USBD register clock, endpoint-event
    // sources cannot be serviced. Leave only the wake-capable aggregate event
    // armed so a stale endpoint event cannot turn suspend into an IRQ storm.
    reg32(USBD_BASE, INTENCLR) = 0xFFFFFFFFUL;
    reg32(USBD_BASE, INTENSET) = USBD_INT_USBEVENT_MASK;
#endif
}

// NVIC priority registers are byte-addressable, one byte per IRQ. The nRF52840
// (Cortex-M4) implements the top 3 bits of the 8-bit field, so a logical
// priority 0..7 lives in bits [7:5].
constexpr uint32_t NVIC_IPR_BASE = 0xE000E400UL;
inline void setNvicPriority(uint32_t irqNumber, uint8_t priority) {
    *reinterpret_cast<volatile uint8_t *>(NVIC_IPR_BASE + irqNumber) =
        static_cast<uint8_t>((priority << 5) & 0xE0U);
}

// RAII mutual-exclusion between foreground USB servicing and the USBD ISR.
//
// Once the USBD interrupt is enabled (the default for every board option except
// the historical promicroserialnosd poll-only override), processBusState() /
// serviceDataIn() / serviceSetup() run from irqHandler() AND can still be
// reached from foreground (Serial.write -> serviceDataIn, flush(), poll() via
// yield()). They share USBD EasyDMA registers, the in-flight flags, and the
// TX-ring consumer cursor, so the two contexts must never overlap.
//
// Masking the USBD IRQ at the NVIC for the duration of a foreground critical
// section gives clean exclusion: the ISR runs at higher priority than any
// foreground code, so it can only start *between* foreground instructions --
// blocking it here means it cannot preempt us, and any event that arrives while
// masked simply stays pending and is taken the instant we unmask. The guard
// saves and restores the prior enable state, so it is a zero-cost no-op when the
// IRQ is disabled (poll-only builds, or while the GDB stub holds it masked) and
// nests correctly.
class UsbdIrqLock {
public:
    UsbdIrqLock() : reenable_(nvicIrqEnabled(USBD_IRQ_NUMBER)) {
        if (reenable_) {
            disableNvicIrq(USBD_IRQ_NUMBER);
            // Ensure the NVIC disable has taken architectural effect before the
            // critical section runs, so no USBD IRQ can still be in flight.
            asm volatile("dsb 0xf" ::: "memory");
            asm volatile("isb 0xf" ::: "memory");
        }
    }
    ~UsbdIrqLock() {
        if (reenable_) {
            enableNvicIrq(USBD_IRQ_NUMBER);
        }
    }
    UsbdIrqLock(const UsbdIrqLock &) = delete;
    UsbdIrqLock &operator=(const UsbdIrqLock &) = delete;

private:
    const bool reenable_;
};

inline uint32_t taskStartEpinOffset(uint8_t endpoint) {
    return TASKS_STARTEPIN_BASE + (static_cast<uint32_t>(endpoint) * 4UL);
}

inline uint32_t taskStartEpoutOffset(uint8_t endpoint) {
    return TASKS_STARTEPOUT_BASE + (static_cast<uint32_t>(endpoint) * 4UL);
}

inline uint32_t eventEndEpinOffset(uint8_t endpoint) {
    return EVENTS_ENDEPIN_BASE + (static_cast<uint32_t>(endpoint) * 4UL);
}

inline uint32_t eventEndEpoutOffset(uint8_t endpoint) {
    return EVENTS_ENDEPOUT_BASE + (static_cast<uint32_t>(endpoint) * 4UL);
}

inline uint32_t epoutPtrOffset(uint8_t endpoint) {
    return EPOUT_PTR_BASE + (static_cast<uint32_t>(endpoint) * USBD_ENDPOINT_CLUSTER_STRIDE);
}

inline uint32_t epoutMaxcntOffset(uint8_t endpoint) {
    return EPOUT_MAXCNT_BASE + (static_cast<uint32_t>(endpoint) * USBD_ENDPOINT_CLUSTER_STRIDE);
}

inline uint32_t epoutAmountOffset(uint8_t endpoint) {
    return EPOUT_AMOUNT_BASE + (static_cast<uint32_t>(endpoint) * USBD_ENDPOINT_CLUSTER_STRIDE);
}

// SIZE.EPOUT[n] (0x4A0 + n*4): reading gives the byte count of the packet
// sitting in the endpoint's internal buffer; WRITING any value is the
// documented "buffer consumed" handshake that lets the endpoint ACK the next
// host OUT. Skipping this write can re-present stale FIFO content as phantom
// packets after every real one on affected nRF52840 hardware.
inline uint32_t sizeEpoutOffset(uint8_t endpoint) {
    return 0x4A0UL + (static_cast<uint32_t>(endpoint) * 4UL);
}

inline uint32_t epinPtrOffset(uint8_t endpoint) {
    return EPIN_PTR_BASE + (static_cast<uint32_t>(endpoint) * USBD_ENDPOINT_CLUSTER_STRIDE);
}

inline uint32_t epinMaxcntOffset(uint8_t endpoint) {
    return EPIN_MAXCNT_BASE + (static_cast<uint32_t>(endpoint) * USBD_ENDPOINT_CLUSTER_STRIDE);
}

inline uint32_t epinAmountOffset(uint8_t endpoint) {
    return EPIN_AMOUNT_BASE + (static_cast<uint32_t>(endpoint) * USBD_ENDPOINT_CLUSTER_STRIDE);
}

inline uint32_t endpointMask(uint8_t endpoint) {
    return 1UL << endpoint;
}

inline uint8_t epAddressIn(uint8_t endpoint) {
    return static_cast<uint8_t>(USB_DIR_IN | endpoint);
}

inline uint8_t epAddressOut(uint8_t endpoint) {
    return static_cast<uint8_t>(USB_DIR_OUT | endpoint);
}

inline void clearEndpointToggle(uint8_t endpointAddress) {
    reg32(USBD_BASE, DTOGGLE) = endpointAddress | (USBD_DTOGGLE_NOP << USBD_DTOGGLE_VALUE_POS);
    reg32(USBD_BASE, DTOGGLE) = endpointAddress | (USBD_DTOGGLE_DATA0 << USBD_DTOGGLE_VALUE_POS);
}

inline void clearEndpointStall(uint8_t endpointAddress) {
    reg32(USBD_BASE, EPSTALL) = endpointAddress;
}

inline void resetEndpointDataState(uint8_t endpointAddress) {
    clearEndpointToggle(endpointAddress);
    clearEndpointStall(endpointAddress);
}

inline bool vbusDetected() {
    return (reg32(POWER_BASE, POWER_USBREGSTATUS) & POWER_USBREGSTATUS_VBUSDETECT) != 0UL;
}

// nRF52840 POWER->USBREGSTATUS bit 1 = OUTPUTRDY (USB regulator stable).
// Nordic's startup order is real VBUS -> ENABLE -> EVENTCAUSE.READY and then
// OUTPUTRDY before the D+ pull-up. OUTPUTRDY is therefore never used as a
// precondition for ENABLE, and a build-time "assume VBUS" policy can never
// authorize the physical pull-up.
constexpr uint32_t POWER_USBREGSTATUS_OUTPUTRDY = (1UL << 1);

inline bool usbPwrRdy() {
    return (reg32(POWER_BASE, POWER_USBREGSTATUS) & POWER_USBREGSTATUS_OUTPUTRDY) != 0UL;
}

// The generated nRF52 errata predicates used by current nrfx read these FICR
// configuration IDs. For nRF52840, ID0 == 0x08; erratum 171 covers every
// revision while 187 starts at revision 1. Current nrfx also marks every
// nRF52833 (0x0D) and nRF52820 (0x10) revision as affected by 187. Unknown
// parts fail closed by not touching undocumented workaround registers.
constexpr uint32_t FICR_CONFIG_ID0_ADDRESS = 0x10000130UL;
constexpr uint32_t FICR_CONFIG_ID1_ADDRESS = 0x10000134UL;

constexpr bool usbErrata171Affected(uint32_t configId0, uint32_t /*configId1*/) {
    return configId0 == 0x08UL;
}

constexpr bool usbErrata166Affected(uint32_t configId0, uint32_t /*configId1*/) {
    // Current Nordic MDK marks every nRF52840 revision as affected, including
    // the observed 0x08/0x03 configuration-id pair.
    return configId0 == 0x08UL;
}

constexpr bool usbErrata187Affected(uint32_t configId0, uint32_t configId1) {
    return (configId0 == 0x08UL && configId1 != 0x00UL) ||
           configId0 == 0x0DUL || configId0 == 0x10UL;
}

constexpr bool usbErrata199Affected(uint32_t configId0, uint32_t /*configId1*/) {
    // Current Nordic MDK marks every nRF52840 revision as affected. Unlike
    // 187/211, revision 0 is included.
    return configId0 == 0x08UL;
}

constexpr bool usbErrata211Affected(uint32_t configId0, uint32_t configId1) {
    // Current Nordic MDK predicates use the same supported-part/revision set
    // as Erratum 187. The lifetime is different: 187 brackets ENABLE -> READY,
    // while 211 must hold ED14=3 until USBD has been disabled.
    return usbErrata187Affected(configId0, configId1);
}

static_assert(usbErrata171Affected(0x08UL, 0x00UL));
static_assert(usbErrata171Affected(0x08UL, 0x05UL));
static_assert(!usbErrata171Affected(0x0DUL, 0x01UL));
static_assert(usbErrata166Affected(0x08UL, 0x00UL));
static_assert(usbErrata166Affected(0x08UL, 0x03UL));
static_assert(!usbErrata166Affected(0x0DUL, 0x03UL));
static_assert(!usbErrata187Affected(0x08UL, 0x00UL));
static_assert(usbErrata187Affected(0x08UL, 0x01UL));
static_assert(usbErrata187Affected(0x0DUL, 0x00UL));
static_assert(usbErrata187Affected(0x10UL, 0x03UL));
static_assert(!usbErrata187Affected(0x09UL, 0x01UL));
static_assert(usbErrata199Affected(0x08UL, 0x00UL));
static_assert(usbErrata199Affected(0x08UL, 0x05UL));
static_assert(!usbErrata199Affected(0x0DUL, 0x01UL));
static_assert(!usbErrata211Affected(0x08UL, 0x00UL));
static_assert(usbErrata211Affected(0x08UL, 0x03UL));
static_assert(usbErrata211Affected(0x0DUL, 0x00UL));

inline bool usbErrata171Applies() {
    return usbErrata171Affected(mem32(FICR_CONFIG_ID0_ADDRESS),
                                mem32(FICR_CONFIG_ID1_ADDRESS));
}

inline bool usbErrata166Applies() {
    return usbErrata166Affected(mem32(FICR_CONFIG_ID0_ADDRESS),
                                mem32(FICR_CONFIG_ID1_ADDRESS));
}

inline bool usbErrata187Applies() {
    return usbErrata187Affected(mem32(FICR_CONFIG_ID0_ADDRESS),
                                mem32(FICR_CONFIG_ID1_ADDRESS));
}

inline bool usbErrata199Applies() {
    return usbErrata199Affected(mem32(FICR_CONFIG_ID0_ADDRESS),
                                mem32(FICR_CONFIG_ID1_ADDRESS));
}

inline bool usbErrata211Applies() {
    return usbErrata211Affected(mem32(FICR_CONFIG_ID0_ADDRESS),
                                mem32(FICR_CONFIG_ID1_ADDRESS));
}

// nRF52840 Errata 187 ("USBD: USB cannot be enabled"): wraps the USBD
// ENABLE handshake with a trim-register sequence at 0x4006EC00 / 0x4006ED14.
// Keep the established TaichiUSB transaction body intact, but call it only
// when the current nrfx dynamic silicon predicate says the anomaly applies.
inline void usbErrata187First() {
    if (*reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) == 0x00000000UL) {
        *reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) = 0x00009375UL;
        *reinterpret_cast<volatile uint32_t *>(0x4006ED14UL) = 0x00000003UL;
        *reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) = 0x00009375UL;
    } else {
        *reinterpret_cast<volatile uint32_t *>(0x4006ED14UL) = 0x00000003UL;
    }
}

inline void usbErrata187Second() {
    if (*reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) == 0x00000000UL) {
        *reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) = 0x00009375UL;
        *reinterpret_cast<volatile uint32_t *>(0x4006ED14UL) = 0x00000000UL;
        *reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) = 0x00009375UL;
    } else {
        *reinterpret_cast<volatile uint32_t *>(0x4006ED14UL) = 0x00000000UL;
    }
}

// nRF52840 Errata 171 ("USBD: USB might not power up"): Nordic's nrfx_usbd
// applies this workaround inside Errata 187 around the ENABLE handshake.
// Its flag lives at 0x4006EC14 (not the 0x4006ED14 used by 187) and uses
// 0xC0 rather than 0x3. The transaction is also revision-gated above.
inline void usbErrata171First() {
    if (*reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) == 0x00000000UL) {
        *reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) = 0x00009375UL;
        *reinterpret_cast<volatile uint32_t *>(0x4006EC14UL) = 0x000000C0UL;
        *reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) = 0x00009375UL;
    } else {
        *reinterpret_cast<volatile uint32_t *>(0x4006EC14UL) = 0x000000C0UL;
    }
}

inline void usbErrata171Second() {
    if (*reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) == 0x00000000UL) {
        *reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) = 0x00009375UL;
        *reinterpret_cast<volatile uint32_t *>(0x4006EC14UL) = 0x00000000UL;
        *reinterpret_cast<volatile uint32_t *>(0x4006EC00UL) = 0x00009375UL;
    } else {
        *reinterpret_cast<volatile uint32_t *>(0x4006EC14UL) = 0x00000000UL;
    }
}

inline void usbErrata199BeginDma() {
    if (usbErrata199Applies()) {
        mem32(USBD_ERRATA199_DMA_PENDING_ADDRESS) = USBD_ERRATA199_DMA_PENDING_VALUE;
    }
}

inline void usbErrata199EndDma() {
    if (usbErrata199Applies()) {
        mem32(USBD_ERRATA199_DMA_PENDING_ADDRESS) = 0UL;
    }
}

// Spin up to ~100 ms (at 64 MHz, ~6 M cycles) waiting for a one-shot USB
// power-state event. Used for both VBUS detection and OUTPUTRDY readiness;
// the actual hardware transitions are sub-millisecond, but we want headroom
// for any ramp-up / debouncing inside the regulator. Returns true on
// observed transition, false on timeout.
constexpr uint32_t USBD_POWER_WAIT_SPINS = 6000000UL;
template <typename Pred>
inline bool spinUntil(Pred pred) {
    for (uint32_t spin = 0; spin < USBD_POWER_WAIT_SPINS; ++spin) {
        if (pred()) {
            return true;
        }
    }
    return false;
}

inline void ensureHfclk() {
    reg32(CLOCK_BASE, CLOCK_EVENTS_HFCLKSTARTED) = 0UL;
    reg32(CLOCK_BASE, CLOCK_TASKS_HFCLKSTART) = 1UL;
    for (uint32_t spin = 0; spin < 200000UL; ++spin) {
        if (reg32(CLOCK_BASE, CLOCK_EVENTS_HFCLKSTARTED) != 0UL) {
            break;
        }
    }
}


constexpr uint32_t USBD_BOOTLOADER_RESET_DISCONNECT_SPINS = 640000UL;

inline void requestBootloaderReset() {
    // Funnel every software-DFU handoff through the driver's normal teardown.
    // Besides detach and errata cleanup, end() accounts the actual AMOUNT of a
    // just-completed EasyDMA and, when the cumulative byte count is odd, runs
    // Nordic's one-byte EPIN0 parity repair before ENABLE can reach zero.
    nrfUsbdDriver().end();
    if (reg32(USBD_BASE, ENABLE) == 0UL) {
        // Give the host a real detach interval only after the hardware has
        // stopped driving the bus, then hand control to the bootloader.
        for (volatile uint32_t spin = 0UL;
             spin < USBD_BOOTLOADER_RESET_DISCONNECT_SPINS;
             ++spin) {
            __asm volatile("nop");
        }
    }
    nrfPrepareBootloaderResetRequest(nrfBootloaderUploadResetMagic());
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
    mem32(AIRCR) = AIRCR_RESET_KEY | AIRCR_SYSRESETREQ;
    __asm volatile("dsb 0xF" ::: "memory");
    while (true) {
    }
}

inline void markResetCause(uint32_t cause) {
    mem32(USBD_DIAG_CAUSE_ADDR) = cause;
}

inline void markResetCauseIfUnset(uint32_t cause) {
    if (mem32(USBD_DIAG_CAUSE_ADDR) == 0UL) {
        markResetCause(cause);
    }
}

inline void diagResetAtUsbdBeginStage(uint32_t stage) {
#if defined(NRF_USBD_DIAG_RESET_STAGE)
    if (static_cast<uint32_t>(NRF_USBD_DIAG_RESET_STAGE) == stage) {
        markResetCause(USBD_DIAG_CAUSE_DIAG_STAGE_BASE | (stage & 0xFFUL));
        requestBootloaderReset();
    }
#else
    (void)stage;
#endif
}

inline size_t copyUsbStringDescriptor(const char *text, uint8_t *buffer, size_t capacity) {
    if (capacity < 2U) {
        return 0U;
    }

    size_t offset = 2U;
    while (text != nullptr && *text != '\0' && (offset + 1U) < capacity) {
        buffer[offset++] = static_cast<uint8_t>(*text++);
        buffer[offset++] = 0U;
    }
    buffer[0] = static_cast<uint8_t>(offset);
    buffer[1] = USB_DESC_STRING;
    return offset;
}

inline void ringPush(uint8_t *buffer, volatile size_t &head, volatile size_t &tail, uint8_t value) {
    const size_t next = (head + 1U) % USBD_RING_BUFFER_SIZE;
    if (next == tail) {
        return;
    }
    buffer[head] = value;
    head = next;
}

inline bool ringPushWithResult(uint8_t *buffer, volatile size_t &head, volatile size_t &tail, uint8_t value) {
    const size_t next = (head + 1U) % USBD_RING_BUFFER_SIZE;
    if (next == tail) {
        return false;
    }
    buffer[head] = value;
    head = next;
    return true;
}

inline int ringPop(uint8_t *buffer, volatile size_t &head, volatile size_t &tail) {
    if (head == tail) {
        return -1;
    }
    const uint8_t value = buffer[tail];
    tail = (tail + 1U) % USBD_RING_BUFFER_SIZE;
    return static_cast<int>(value);
}

inline int ringPeek(const uint8_t *buffer, volatile size_t head, volatile size_t tail) {
    if (head == tail) {
        return -1;
    }
    return static_cast<int>(buffer[tail]);
}

inline size_t ringPending(volatile size_t head, volatile size_t tail) {
    if (head >= tail) {
        return head - tail;
    }
    return (USBD_RING_BUFFER_SIZE - tail) + head;
}

NrfUsbdPollTrace g_usbdPollTrace __attribute__((section(".noinit")));

bool pollTraceEnabled() {
    return g_usbdPollTrace.magic == USBD_TRACE_MAGIC;
}

void zeroPollTrace() {
    g_usbdPollTrace.pollCalls = 0UL;
    g_usbdPollTrace.irqCalls = 0UL;
    g_usbdPollTrace.ep0SetupEvents = 0UL;
    g_usbdPollTrace.ep0DataDoneEvents = 0UL;
    g_usbdPollTrace.cdcOutEvents = 0UL;
    g_usbdPollTrace.cdcInEvents = 0UL;
    g_usbdPollTrace.usbEventEvents = 0UL;
    g_usbdPollTrace.lastEventCause = 0UL;
}
}

NrfUsbdDriver &nrfUsbdDriver() {
    static NrfUsbdDriver driver;
    return driver;
}

const NrfUsbdPollTrace &nrfUsbdPollTrace() {
    return g_usbdPollTrace;
}

void nrfUsbdResetPollTrace() {
    g_usbdPollTrace.magic = USBD_TRACE_MAGIC;
    zeroPollTrace();
}

void nrfUsbdClearPollTrace() {
    g_usbdPollTrace.magic = 0UL;
    zeroPollTrace();
}

bool NrfUsbdDriver::startEasyDma(DmaDirection direction, uint8_t endpoint) {
    if (direction == DmaDirection::None || endpoint >= USB_MAX_ENDPOINTS ||
        terminalFault_ || dmaDirection_ != DmaDirection::None) {
        return false;
    }

    const uint32_t endEvent = direction == DmaDirection::In
        ? eventEndEpinOffset(endpoint)
        : eventEndEpoutOffset(endpoint);
    const uint32_t startTask = direction == DmaDirection::In
        ? taskStartEpinOffset(endpoint)
        : taskStartEpoutOffset(endpoint);

    reg32(USBD_BASE, endEvent) = 0UL;
    reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
    dmaDirection_ = direction;
    dmaEndpoint_ = endpoint;

    // nRF52840 Erratum 199: 0x40027C1C must be 0x82 from immediately
    // before every EasyDMA task until the matching END event. The class-level
    // owner also enforces the USBD peripheral's single shared DMA channel.
    usbErrata199BeginDma();
    reg32(USBD_BASE, startTask) = 1UL;
    for (uint32_t spin = 0UL; spin < USBD_START_CAPTURE_TIMEOUT_SPINS; ++spin) {
        if (reg32(USBD_BASE, EVENTS_STARTED) != 0UL) {
            const uint32_t captureMask = direction == DmaDirection::In
                ? (1UL << endpoint)
                : (1UL << (EPSTATUS_OUT_BASE_BIT + endpoint));
            // EPSTATUS is W1C. A stale capture bit would otherwise make a
            // later STARTED event ambiguous even though the END event is per-EP.
            reg32(USBD_BASE, EPSTATUS) = captureMask;
            reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
            return true;
        }
    }

    // Capture state is unknowable after the timeout. Keep the Erratum 199
    // value and ownership asserted until a confirmed peripheral disable.
    terminalFault_ = true;
    reg32(USBD_BASE, USBPULLUP) = 0UL;
    disableInterrupts();
    return false;
}

bool NrfUsbdDriver::completeEasyDma(DmaDirection direction, uint8_t endpoint) {
    if (dmaDirection_ != direction || dmaEndpoint_ != endpoint) {
        return false;
    }
    const uint32_t endEvent = direction == DmaDirection::In
        ? eventEndEpinOffset(endpoint)
        : eventEndEpoutOffset(endpoint);
    if (reg32(USBD_BASE, endEvent) == 0UL) {
        return false;
    }

    // Track the hardware-reported bytes, not MAXCNT and not the requested
    // length. This is Nordic nrfx's cumulative EasyDMA parity invariant: an
    // odd total must be repaired before the next USBD disable/re-enable.
    const uint32_t amount = direction == DmaDirection::In
        ? reg32(USBD_BASE, epinAmountOffset(endpoint))
        : reg32(USBD_BASE, epoutAmountOffset(endpoint));
    dmaBytesOdd_ = dmaParityAfterAmount(dmaBytesOdd_, amount);
    usbErrata199EndDma();
    dmaDirection_ = DmaDirection::None;
    dmaEndpoint_ = 0xFFU;
    return true;
}

bool NrfUsbdDriver::finishEasyDmaBeforeDisable() {
    if (dmaDirection_ == DmaDirection::None) {
        return true;
    }

    const DmaDirection direction = dmaDirection_;
    const uint8_t endpoint = dmaEndpoint_;
    const uint32_t endEvent = direction == DmaDirection::In
        ? eventEndEpinOffset(endpoint)
        : eventEndEpoutOffset(endpoint);
    for (uint32_t spin = 0UL; spin < USBD_DISABLE_TIMEOUT_SPINS; ++spin) {
        if (reg32(USBD_BASE, endEvent) != 0UL) {
            const bool completed = completeEasyDma(direction, endpoint);
            reg32(USBD_BASE, endEvent) = 0UL;
            return completed;
        }
        if (!vbusDetected() || !usbPwrRdy() ||
            reg32(USBD_BASE, LOWPOWER) != 0UL) {
            break;
        }
    }
    return false;
}

bool NrfUsbdDriver::repairEasyDmaParityBeforeDisable() {
    if (!dmaBytesOdd_) {
        return true;
    }
    if (dmaDirection_ != DmaDirection::None ||
        reg32(USBD_BASE, ENABLE) == 0UL ||
        reg32(USBD_BASE, LOWPOWER) != 0UL ||
        !vbusDetected() || !usbPwrRdy()) {
        return false;
    }

    // Current Nordic nrfx sends one byte through EPIN0 when cumulative
    // completed EasyDMA bytes are odd. It prevents an invalid bus request on
    // the next USBD enable. Run it synchronously with the IRQ masked and a
    // strict bound; never issue this DMA while LOWPOWER owns the register clock
    // or while the USB regulator lacks real VBUS/OUTPUTRDY.
    reg32(USBD_BASE, EPINEN) |= endpointMask(0U);
    reg32(USBD_BASE, epinPtrOffset(0U)) =
        reinterpret_cast<uint32_t>(&dmaParityScratch_);
    reg32(USBD_BASE, epinMaxcntOffset(0U)) = 1UL;
    reg32(USBD_BASE, eventEndEpinOffset(0U)) = 0UL;
    reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
    // Recheck at the task edge as a cable-removal race barrier. No Erratum-199
    // ownership has been asserted yet, so this exit needs no hidden cleanup.
    if (!vbusDetected() || !usbPwrRdy() ||
        reg32(USBD_BASE, LOWPOWER) != 0UL) {
        return false;
    }
    dmaDirection_ = DmaDirection::In;
    dmaEndpoint_ = 0U;
    usbErrata199BeginDma();
    reg32(USBD_BASE, taskStartEpinOffset(0U)) = 1UL;

    for (uint32_t spin = 0UL; spin < USBD_PARITY_REPAIR_TIMEOUT_SPINS; ++spin) {
        if (reg32(USBD_BASE, eventEndEpinOffset(0U)) != 0UL) {
            const uint32_t amount = reg32(USBD_BASE, epinAmountOffset(0U));
            const bool completed = completeEasyDma(DmaDirection::In, 0U);
            reg32(USBD_BASE, eventEndEpinOffset(0U)) = 0UL;
            reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
            return completed && amount == 1UL && !dmaBytesOdd_;
        }
        if (!vbusDetected() || !usbPwrRdy() ||
            reg32(USBD_BASE, LOWPOWER) != 0UL) {
            // The repair DMA's completion is no longer knowable. Keep Erratum
            // 199 and ENABLE ownership intact; only a system reset may recover.
            return false;
        }
    }
    // Same fail-closed ownership rule as startEasyDma(): do not clear the 199
    // marker when the END event was never observed.
    return false;
}

void NrfUsbdDriver::abortEasyDmaAfterBusReset() {
    // USBRESET terminates endpoint transactions in the USBD MAC. It is the
    // only enabled-session event, besides a matching END, that permits us to
    // release an arbitrary in-flight EasyDMA ownership record.
    if (dmaDirection_ != DmaDirection::None) {
        const DmaDirection direction = dmaDirection_;
        const uint8_t endpoint = dmaEndpoint_;
        const uint32_t endEvent = direction == DmaDirection::In
            ? eventEndEpinOffset(endpoint)
            : eventEndEpoutOffset(endpoint);
        if (reg32(USBD_BASE, endEvent) != 0UL) {
            (void)completeEasyDma(direction, endpoint);
            reg32(USBD_BASE, endEvent) = 0UL;
            return;
        }
    }
    usbErrata199EndDma();
    dmaDirection_ = DmaDirection::None;
    dmaEndpoint_ = 0xFFU;
}

void NrfUsbdDriver::clearEasyDmaAfterConfirmedDisable() {
    if (reg32(USBD_BASE, ENABLE) != 0UL) {
        return;
    }
    usbErrata199EndDma();
    dmaDirection_ = DmaDirection::None;
    dmaEndpoint_ = 0xFFU;
}

void NrfUsbdDriver::acknowledgeWakeAndDeferUsbCauses() {
    // Clear the aggregate event first, then W1C only USBWUALLOWED. If another
    // cause arrived before that aggregate clear, its hardware cause bit remains
    // set but its notification edge may have been consumed. Snapshot all such
    // non-wake causes into software so processUsbEvent() handles them even when
    // EVENTS_USBEVENT is now zero. A cause arriving after the snapshot relatches
    // the aggregate event and follows the normal path. No endpoint/configuration
    // register is touched while the controller is low-power or waking.
    reg32(USBD_BASE, EVENTS_USBEVENT) = 0UL;
    reg32(USBD_BASE, EVENTCAUSE) = USBD_EVENTCAUSE_USBWUALLOWED_MASK;
    deferredUsbCauses_ |= reg32(USBD_BASE, EVENTCAUSE) &
        ~USBD_EVENTCAUSE_USBWUALLOWED_MASK;
}

bool NrfUsbdDriver::wakeFromLowPower() {
    if (reg32(USBD_BASE, LOWPOWER) == 0UL) {
        return true;
    }
    if (!vbusDetected()) {
        return false;
    }

    // USBWUALLOWED is W1C. A stale acknowledgement must not satisfy this
    // wake. Erratum 171 brackets LOWPOWER->ForceNormal until the controller
    // explicitly reports that its register clock is usable again.
    acknowledgeWakeAndDeferUsbCauses();
    const bool applyErrata171 = usbErrata171Applies();
    if (applyErrata171) {
        usbErrata171First();
    }
    wakePending_ = true;
    reg32(USBD_BASE, LOWPOWER) = 0UL;
    const uint32_t lowPowerReadback = reg32(USBD_BASE, LOWPOWER);
    (void)lowPowerReadback;

    const bool wakeWaitCompleted = spinUntil([]() {
        return !vbusDetected() ||
               (reg32(USBD_BASE, EVENTCAUSE) &
                USBD_EVENTCAUSE_USBWUALLOWED_MASK) != 0UL;
    });
    const bool wakeAllowed = wakeWaitCompleted && vbusDetected() &&
        (reg32(USBD_BASE, EVENTCAUSE) &
         USBD_EVENTCAUSE_USBWUALLOWED_MASK) != 0UL;
    if (!wakeAllowed) {
        // Keep Erratum 171 open. disablePeripheral() closes it only after
        // ENABLE is proven zero, and wakePending_ prevents premature DP/DM or
        // endpoint-register access while the wake acknowledgement is unknown.
        return false;
    }

    acknowledgeWakeAndDeferUsbCauses();
    if (applyErrata171) {
        usbErrata171Second();
    }
    wakePending_ = false;
    return true;
}

bool NrfUsbdDriver::disablePeripheral() {
    disableNvicIrq(USBD_IRQ_NUMBER);
    clearPendingNvicIrq(USBD_IRQ_NUMBER);

    // Parity repair needs the shared EasyDMA channel and accessible endpoint
    // registers. If an odd session is suspended, first use the established
    // Erratum-171-protected wake; never start the repair in LOWPOWER itself.
    if (reg32(USBD_BASE, ENABLE) != 0UL && dmaBytesOdd_ &&
        !wakePending_ && reg32(USBD_BASE, LOWPOWER) != 0UL &&
        vbusDetected()) {
        (void)wakeFromLowPower();
    }
    const bool controllerAccessible = reg32(USBD_BASE, ENABLE) != 0UL &&
        !wakePending_ && reg32(USBD_BASE, LOWPOWER) == 0UL;
    bool dmaSettled = dmaDirection_ == DmaDirection::None;
    bool paritySafe = !dmaBytesOdd_;
    if (controllerAccessible) {
        // These registers are unavailable once ENABLE reaches zero. Release
        // forced line ownership and normalize the active state only when the
        // controller is already in ForceNormal. Never manufacture an
        // unprotected wake merely to tear the peripheral down.
        reg32(USBD_BASE, USBPULLUP) = 0UL;
        reg32(USBD_BASE, INTENCLR) = 0xFFFFFFFFUL;

        // A latched END belongs to this session and must contribute its actual
        // AMOUNT before parity is evaluated. A still-pending OUT DMA may wait
        // for a host packet forever, so the wait is bounded. It must never be
        // aborted by ENABLE=0 or displaced by the one-byte repair while it
        // still owns EasyDMA; an unsettled transfer fails closed below.
        dmaSettled = finishEasyDmaBeforeDisable();
        paritySafe = dmaSettled
            ? repairEasyDmaParityBeforeDisable()
            : !dmaBytesOdd_;

        if (!dmaDisableMayCommit(!wakePending_, dmaSettled,
                                 paritySafe && !dmaBytesOdd_)) {
            // Unknown/active EasyDMA, an unacknowledged wake, or a failed odd
            // repair must keep the current USBD session alive. In particular,
            // do not clear Errata 199/211 or write ENABLE=0: only a system reset
            // can safely recover once the exact DMA parity is unknowable.
            terminalFault_ = true;
            return false;
        }

        reg32(USBD_BASE, TASKS_DPDMNODRIVE) = 1UL;
        reg32(USBD_BASE, SHORTS) = 0UL;
        reg32(USBD_BASE, DPDMVALUE) = 0UL;
        reg32(USBD_BASE, EPINEN) = 0UL;
        reg32(USBD_BASE, EPOUTEN) = 0UL;
        reg32(USBD_BASE, ISOSPLIT) = 0UL;
        reg32(USBD_BASE, ISOINCONFIG) = 0UL;
    } else if (reg32(USBD_BASE, ENABLE) != 0UL &&
               !dmaDisableMayCommit(!wakePending_, dmaSettled, paritySafe)) {
        // LOWPOWER makes endpoint registers inaccessible. An already-even,
        // idle session may still be disabled directly, but never guess when a
        // DMA owner, odd parity, or an unfinished protected wake remains.
        terminalFault_ = true;
        return false;
    }
    reg32(USBD_BASE, ENABLE) = 0UL;
    for (uint32_t spin = 0UL;
         spin < USBD_DISABLE_TIMEOUT_SPINS && reg32(USBD_BASE, ENABLE) != 0UL;
         ++spin) {
    }
    if (reg32(USBD_BASE, ENABLE) != 0UL) {
        // Do not clear Errata 199/211 or retry against a peripheral whose
        // disable never completed.
        terminalFault_ = true;
        return false;
    }

    clearEasyDmaAfterConfirmedDisable();
    if (usbErrata171Applies()) {
        usbErrata171Second();
    }
    wakePending_ = false;
    deferredUsbCauses_ = 0UL;
    if (usbErrata211Applies()) {
        usbErrata187Second();
    }
    if (!paritySafe || dmaBytesOdd_) {
        // The peripheral is electrically off, but re-enabling it after an odd
        // cumulative DMA count is precisely the invalid-bus-request condition
        // Nordic's repair prevents. Keep this driver fail-closed until reset.
        terminalFault_ = true;
        return false;
    }
    return true;
}

void NrfUsbdDriver::sanitizeEnabledSession() {
    // A SoftDevice bootloader can leave an enabled peripheral with stale line
    // forcing, endpoint configuration, events, and W1C status. Run this only
    // after this application has produced its own ENABLE->READY edge, with D+
    // still detached and before any EasyDMA transaction or interrupt is armed.
    disableInterrupts();
    reg32(USBD_BASE, USBPULLUP) = 0UL;
    // begin()/wakeFromLowPower() has already proven ForceNormal. Releasing a
    // bootloader's forced DP/DM state before that acknowledgement can be
    // ignored by hardware and strand the new session detached.
    reg32(USBD_BASE, TASKS_DPDMNODRIVE) = 1UL;
    reg32(USBD_BASE, SHORTS) = 0UL;
    reg32(USBD_BASE, DPDMVALUE) = 0UL;
    reg32(USBD_BASE, EPINEN) = 0UL;
    reg32(USBD_BASE, EPOUTEN) = 0UL;

    // Mirror current nrfx post-READY configuration. Erratum 166 applies to
    // every nRF52840 factory revision (including 0x08/0x03) and must precede
    // ISO buffer configuration. This stack does not expose ISO endpoints yet,
    // but leaving the controller in a different undocumented state makes later
    // CDC-only enable/disable cycles diverge from Nordic's validated baseline.
    if (usbErrata166Applies()) {
        mem32(USBD_ERRATA166_INDEX_ADDRESS) = 0x000007E3UL;
        mem32(USBD_ERRATA166_VALUE_ADDRESS) = 0x00000040UL;
        __asm volatile("isb 0xF" ::: "memory");
        __asm volatile("dsb 0xF" ::: "memory");
    }
    reg32(USBD_BASE, ISOSPLIT) = USBD_ISOSPLIT_HALF_VALUE;
    reg32(USBD_BASE, ISOINCONFIG) = 0UL;
    clearEvents();
    reg32(USBD_BASE, EPSTATUS) = 0xFFFFFFFFUL;
    reg32(USBD_BASE, EPDATASTATUS) = 0xFFFFFFFFUL;
    usbErrata199EndDma();
    dmaDirection_ = DmaDirection::None;
    dmaEndpoint_ = 0xFFU;
    dmaBytesOdd_ = false;
}

void NrfUsbdDriver::begin() {
    if (!nrfUsbRuntimeEnabled() || enabled_ || terminalFault_) {
        return;
    }

    if (!initialized_) {
        diagResetAtUsbdBeginStage(1UL);
        initialized_ = true;
        attached_ = true;
        initDescriptors();
        resetConnectionState();
        diagResetAtUsbdBeginStage(2UL);
    }
    if (!attached_ || !vbusDetected()) {
        // Initialization may happen while battery-powered, or VBUS can vanish
        // during a bootloader jump. Do not leave an inherited peripheral or D+
        // drive active in either case; retry only after real VBUS returns.
        if (reg32(USBD_BASE, ENABLE) != 0UL ||
            reg32(USBD_BASE, USBPULLUP) != 0UL) {
            (void)disablePeripheral();
        }
        startupRetryMillis_ = millis();
        return;
    }

    ensureHfclk();

    // Always establish a verified disabled baseline. This handles both an
    // enabled SoftDevice bootloader handoff and the subtler case where ENABLE
    // is already zero but bootloader interrupts/pending state survived. Keep a
    // short host-visible detach interval only after the readback reached zero;
    // no wait is held under an IRQ/critical-section lock.
    if (!disablePeripheral()) {
        return;
    }
    const uint32_t detachStartMillis = millis();
    while ((millis() - detachStartMillis) < USBD_HANDOFF_DETACH_MS) {
        // Do not call delay()/yield() here: yield() polls this same driver and
        // could recursively enter begin() while enabled_ is still false.
    }

    // Nordic order: real VBUS was checked above; now clear stale READY before
    // entering the revision-gated workaround transaction and setting ENABLE.
    clearEvents();
    diagResetAtUsbdBeginStage(3UL);
    const bool applyErrata187 = usbErrata187Applies();
    const bool applyErrata171 = usbErrata171Applies();
    const bool applyErrata211 = usbErrata211Applies();
    if (applyErrata187) {
        usbErrata187First();
    }
    if (applyErrata171) {
        usbErrata171First();
    }

    reg32(USBD_BASE, ENABLE) = USBD_ENABLE_VALUE;
    diagResetAtUsbdBeginStage(4UL);

    // READY is a mandatory post-ENABLE event. Also observe a VBUS loss while
    // waiting so a cable race exits promptly instead of being treated as a
    // successful startup.
    const bool readyWaitCompleted = spinUntil([]() {
        return !vbusDetected() ||
               (reg32(USBD_BASE, EVENTCAUSE) & USBD_EVENTCAUSE_READY_MASK) != 0UL;
    });
    const bool readyObserved = readyWaitCompleted && vbusDetected() &&
        (reg32(USBD_BASE, EVENTCAUSE) & USBD_EVENTCAUSE_READY_MASK) != 0UL;

    if (readyObserved) {
        // These are the first guaranteed-access session writes. Keep D+
        // detached and consume READY before closing the enable workaround;
        // LOWPOWER recovery and DPDMNODRIVE follow as separate phases below.
        reg32(USBD_BASE, USBPULLUP) = 0UL;
        reg32(USBD_BASE, EVENTCAUSE) = USBD_EVENTCAUSE_READY_MASK;
    }

    // Close the nested workaround transaction on every path, in reverse order.
    if (applyErrata171) {
        usbErrata171Second();
    }
    if (applyErrata187) {
        usbErrata187Second();
    }
    if (!readyObserved) {
        failClosedPeripheral();
        return;
    }

    // Errata 187 and 211 share ED14 but have distinct lifetimes. Nordic nrfx
    // closes the 187 ENABLE transaction above, then reopens ED14 for 211 and
    // keeps it at 3 for the complete enabled lifetime. This is required for
    // correct resume/SOF behavior on affected nRF52840 revisions; it is kept
    // separate from the cold-attach diagnosis below.
    if (applyErrata211) {
        usbErrata187First();
    }
    // An enabled bootloader session can hand LOWPOWER across reset. READY only
    // proves that ENABLE completed; ForceNormal has its own protected
    // USBWUALLOWED acknowledgement. Do not release DP/DM ownership or touch
    // session registers until that acknowledgement is observed.
    if (!wakeFromLowPower()) {
        failClosedPeripheral();
        return;
    }
    sanitizeEnabledSession();
    diagResetAtUsbdBeginStage(5UL);

    // OUTPUTRDY is checked after ENABLE and READY. Timeouts and VBUS races are
    // failures, not warnings: the retry starts later from ENABLE=0/PULLUP=0.
    const bool outputWaitCompleted = spinUntil([]() {
        return !vbusDetected() || usbPwrRdy();
    });
    if (!outputWaitCompleted || !vbusDetected() || !usbPwrRdy()) {
        failClosedPeripheral();
        return;
    }

    // Final real-VBUS check before publishing ready state or enabling anything
    // visible to the host. enablePullup() repeats the check as a race barrier.
    if (!vbusDetected()) {
        failClosedPeripheral();
        return;
    }
    enabled_ = true;
    resetConnectionState();
    started_ = true;
    ready_ = true;
    enableInterrupts();
    enablePullup(attached_);
    if (!vbusDetected()) {
        failClosedPeripheral();
        return;
    }
    configStartMillis_ = millis();
    diagResetAtUsbdBeginStage(6UL);
}

void NrfUsbdDriver::failClosedPeripheral() {
    // This helper is used for readiness timeouts, unplug races, and retries.
    // It deliberately preserves initialized_/attached_ so poll() can retry,
    // while all host-visible and peripheral-active state is cleared.
    const bool disabled = disablePeripheral();
    enabled_ = false;
    started_ = false;
    ready_ = false;
    resetConnectionState();
    ready_ = false;
    eventCause_ = 0UL;
    if (disabled && !terminalFault_) {
        startupRetryMillis_ = millis();
    }
}

void NrfUsbdDriver::end() {
    failClosedPeripheral();
    if (reg32(USBD_BASE, ENABLE) == 0UL && !dmaBytesOdd_) {
        // An explicit end()/begin() cycle is allowed to recover after the
        // peripheral has reached the one state that makes errata cleanup safe.
        terminalFault_ = false;
    }
    initialized_ = false;
    attached_ = false;
    pendingControlOut_ = ControlOutTransfer::None;
    controlOutExpected_ = 0U;
    controlOutLength_ = 0U;
    rxHead_ = 0U;
    rxTail_ = 0U;
    txHead_ = 0U;
    txTail_ = 0U;
    userRxHead_ = 0U;
    userRxTail_ = 0U;
    userTxHead_ = 0U;
    userTxTail_ = 0U;
    eventCause_ = 0UL;
    serialStateBitmap_ = 0U;
    userSerialStateBitmap_ = 0U;
}

void NrfUsbdDriver::attach() {
    if (!nrfUsbRuntimeEnabled()) {
        return;
    }
    if (!initialized_) {
        begin();
        return;
    }
    attached_ = true;
    if (!enabled_) {
        begin();
        return;
    }

    resetConnectionState();
    clearEvents();
    started_ = true;
    enablePullup(vbusDetected());
}

void NrfUsbdDriver::detach() {
    if (!enabled_) {
        attached_ = false;
        return;
    }

    // A logical detach is also the safe cancellation boundary for any pending
    // EasyDMA operation. Use the same verified disable path as error recovery;
    // attach() will create a fresh sanitized enabled session.
    attached_ = false;
    failClosedPeripheral();
}

void NrfUsbdDriver::poll() {
    if (!initialized_) {
        return;
    }
    if (terminalFault_) {
        if (enabled_) {
            failClosedPeripheral();
        }
        return;
    }
    if (!enabled_) {
        const uint32_t now = millis();
        if (attached_ && vbusDetected() &&
            (now - startupRetryMillis_) >= USBD_STARTUP_RETRY_MS) {
            // Retry is foreground-driven and rate-limited; each attempt remains
            // bounded and fail-closed if READY/OUTPUTRDY does not arrive.
            begin();
        }
        return;
    }
    if (!vbusDetected()) {
        failClosedPeripheral();
        return;
    }

    if (pollTraceEnabled()) {
        ++g_usbdPollTrace.pollCalls;
    }

    // Serialize against the USBD ISR: poll() and irqHandler() run the same
    // servicing routines and touch the same registers/state. With the IRQ live
    // (interrupt-driven builds) this mask makes the whole foreground pass atomic
    // w.r.t. the ISR; on poll-only builds the IRQ is disabled and this is a
    // no-op.
    UsbdIrqLock lock;

    const bool hasVbus = vbusDetected();
    processBusState(hasVbus);
    if (enabled_ && !suspended_) {
        enablePullup(attached_ && hasVbus);
    }

    if (USBD_CONFIG_TIMEOUT_RESET_MS != 0UL && attached_ && hasVbus && !configured_ && configStartMillis_ != 0UL &&
        (millis() - configStartMillis_) >= USBD_CONFIG_TIMEOUT_RESET_MS) {
        markResetCause(USBD_DIAG_CAUSE_CONFIG_TIMEOUT);
        requestBootloaderReset();
    }

    if (attached_ && hasVbus) {
        started_ = true;
    }

    if (attached_ && configured_ && !suspended_) {
        serviceDataIn(false);
        serviceNotificationIn(false);
        if (userPortEnabled()) {
            serviceDataIn(true);
            serviceNotificationIn(true);
        }
        serviceDynamicEndpoints();
    }

    if (serviceTouchPending_) {
        const bool resetArmed = configuredMillis_ != 0UL &&
            (millis() - configuredMillis_) >= USBD_1200_RESET_ARM_MS;
        const bool windowStarted = serviceTouchResetMillis_ != 0UL;
        const bool windowElapsed = windowStarted &&
            (millis() - serviceTouchResetMillis_) >= USBD_TOUCH_RESET_CONFIRM_MS;
        const bool inConfirmWindow = windowStarted && !windowElapsed;
        if (windowElapsed) {
            // The confirm window opened and fully elapsed: commit the touch and
            // reboot. We deliberately do NOT re-check DTR here. Windows
            // re-asserts DTR on the port close that immediately follows the
            // touch, so if poll()/the ISR first runs AFTER the window elapsed
            // with DTR back high, re-checking it would cancel an already-armed
            // touch. This hazard is largest with usbcdc=disabled, where the
            // single service CDC sees little traffic and the ISR can land
            // outside the short confirm window.
            serviceTouchPending_ = false;
            serviceTouchResetMillis_ = 0UL;
            if (ignoredResetTouchCount_ < USBD_IGNORE_INITIAL_1200_RESET_COUNT) {
                ++ignoredResetTouchCount_;
            } else {
                detachCause_ = USBD_DIAG_CAUSE_1200_TOUCH;
                detachRequestMagic_ = USBD_DETACH_REQUEST_MAGIC;
            }
        } else if (!configured_ ||
                   (dtr_ && !inConfirmWindow) || lineCoding_.baudRate != 1200UL) {
            // NOTE: the 1200-bps-touch reboot is honored regardless of the build's
            // default upload mode (it used to be gated on prefersUsbUpload). The
            // touch is the universal DFU *recovery* path; disabling it on a
            // jlink/openocd-profile build means one wrong-profile flash can leave a
            // board with no host-side way back into the bootloader. It still only
            // fires on a configured port seeing 1200 baud + a DTR drop on the
            // service CDC, which never happens for normal data.
            serviceTouchPending_ = false;
            serviceTouchResetMillis_ = 0UL;
        } else if (!resetArmed) {
            serviceTouchResetMillis_ = 0UL;
        } else if (serviceTouchResetMillis_ == 0UL) {
            serviceTouchResetMillis_ = millis();
        }
    }

    if (detachRequestMagic_ == USBD_DETACH_REQUEST_MAGIC) {
        const uint32_t cause = detachCause_;
        detachRequestMagic_ = 0UL;
        detachCause_ = 0UL;
        diagResetAtUsbdBeginStage(22UL);
        if (cause != 0UL) {
            markResetCause(cause);
            requestBootloaderReset();
        }
    }
    diagResetAtUsbdBeginStage(23UL);
}

void NrfUsbdDriver::irqHandler() {
    if (!enabled_ || terminalFault_) {
        return;
    }

    if (pollTraceEnabled()) {
        ++g_usbdPollTrace.irqCalls;
    }

    processBusState(vbusDetected());
    if (attached_ && configured_ && !suspended_) {
        serviceDataIn(false);
        serviceNotificationIn(false);
        if (userPortEnabled()) {
            serviceDataIn(true);
            serviceNotificationIn(true);
        }
        serviceDynamicEndpoints();
    }
    if (serviceTouchPending_) {
        const bool resetArmed = configuredMillis_ != 0UL &&
            (millis() - configuredMillis_) >= USBD_1200_RESET_ARM_MS;
        const bool windowStarted = serviceTouchResetMillis_ != 0UL;
        const bool windowElapsed = windowStarted &&
            (millis() - serviceTouchResetMillis_) >= USBD_TOUCH_RESET_CONFIRM_MS;
        const bool inConfirmWindow = windowStarted && !windowElapsed;
        if (windowElapsed) {
            // The confirm window opened and fully elapsed: commit the touch and
            // reboot. We deliberately do NOT re-check DTR here. Windows
            // re-asserts DTR on the port close that immediately follows the
            // touch, so if poll()/the ISR first runs AFTER the window elapsed
            // with DTR back high, re-checking it would cancel an already-armed
            // touch. This hazard is largest with usbcdc=disabled, where the
            // single service CDC sees little traffic and the ISR can land
            // outside the short confirm window.
            serviceTouchPending_ = false;
            serviceTouchResetMillis_ = 0UL;
            if (ignoredResetTouchCount_ < USBD_IGNORE_INITIAL_1200_RESET_COUNT) {
                ++ignoredResetTouchCount_;
            } else {
                detachCause_ = USBD_DIAG_CAUSE_1200_TOUCH;
                detachRequestMagic_ = USBD_DETACH_REQUEST_MAGIC;
            }
        } else if (!configured_ ||
                   (dtr_ && !inConfirmWindow) || lineCoding_.baudRate != 1200UL) {
            // NOTE: the 1200-bps-touch reboot is honored regardless of the build's
            // default upload mode (it used to be gated on prefersUsbUpload). The
            // touch is the universal DFU *recovery* path; disabling it on a
            // jlink/openocd-profile build means one wrong-profile flash can leave a
            // board with no host-side way back into the bootloader. It still only
            // fires on a configured port seeing 1200 baud + a DTR drop on the
            // service CDC, which never happens for normal data.
            serviceTouchPending_ = false;
            serviceTouchResetMillis_ = 0UL;
        } else if (!resetArmed) {
            serviceTouchResetMillis_ = 0UL;
        } else if (serviceTouchResetMillis_ == 0UL) {
            serviceTouchResetMillis_ = millis();
        }
    }
    if (detachRequestMagic_ == USBD_DETACH_REQUEST_MAGIC) {
        const uint32_t cause = detachCause_;
        detachRequestMagic_ = 0UL;
        detachCause_ = 0UL;
        if (cause != 0UL) {
            markResetCause(cause);
            requestBootloaderReset();
        }
    }
}

// Number of consecutive halted-pump iterations the host must hold the service
// CDC open at 1200 bps before we reboot to the bootloader. The stub busy-loops
// the pump, so this is a debounce against a transient line-coding readout, not
// a wall-clock interval (millis() is frozen during the DebugMon halt).
constexpr uint32_t USBD_HALT_TOUCH_CONFIRM_TICKS = 1024UL;

void NrfUsbdDriver::setStubHalted(bool halted) {
    stubHalted_ = halted;
    haltTouchTicks_ = 0UL;
}

void NrfUsbdDriver::serviceHaltedTouch() {
    // Only meaningful while the GDB stub is pumping us from its halted loop.
    if (!stubHalted_) {
        haltTouchTicks_ = 0UL;
        return;
    }
    // The host opens the service CDC at exactly 1200 bps solely to request the
    // DFU touch; nothing in a debug session legitimately does. lineCoding_ is
    // updated from EP0 control-OUT (completeControlOutTransfer), which still
    // completes while halted — that is how re-enumeration finishes — so this
    // signal is observable even though millis() is frozen.
    // Honor the touch even while halted in the GDB stub debugger, on any profile -
    // a debug build must still be DFU-recoverable over USB.
    const bool touchSignal = enabled_ && configured_ &&
        lineCoding_.baudRate == 1200UL;
    if (!touchSignal) {
        haltTouchTicks_ = 0UL;
        return;
    }
    if (++haltTouchTicks_ >= USBD_HALT_TOUCH_CONFIRM_TICKS) {
        // Reboot into the bootloader so the upload's DFU write lands on the
        // bootloader (which programs flash safely) instead of stalling against a
        // halted app and leaving a partially-written, bricked image.
        markResetCause(USBD_DIAG_CAUSE_1200_TOUCH);
        requestBootloaderReset();  // never returns
    }
}

bool NrfUsbdDriver::processUsbEvent(bool hasVbus) {
    const uint32_t deferredCauses = deferredUsbCauses_;
    const bool aggregateEvent =
        reg32(USBD_BASE, EVENTS_USBEVENT) != 0UL;
    if (!aggregateEvent && deferredCauses == 0UL) {
        return !suspended_;
    }

    // EVENTS_USBEVENT is a normal event register (write zero to clear), while
    // EVENTCAUSE is W1C. Snapshot and acknowledge the causes before a SUSPEND
    // transition makes most USBD registers inaccessible. A later cause will
    // relatch the aggregate event and is handled on the next pass.
    if (aggregateEvent) {
        reg32(USBD_BASE, EVENTS_USBEVENT) = 0UL;
    }
    const uint32_t causes =
        reg32(USBD_BASE, EVENTCAUSE) | deferredCauses;
    deferredUsbCauses_ = 0UL;
    eventCause_ = causes;
    if (pollTraceEnabled()) {
        ++g_usbdPollTrace.usbEventEvents;
        g_usbdPollTrace.lastEventCause = causes;
    }
    if (causes != 0UL) {
        reg32(USBD_BASE, EVENTCAUSE) = causes;
    }

    if ((causes & USBD_EVENTCAUSE_READY_MASK) != 0UL) {
        ready_ = attached_ && hasVbus;
    }
    if (!hasVbus) {
        failClosedPeripheral();
        return false;
    }

    const bool sawSuspend =
        (causes & USBD_EVENTCAUSE_SUSPEND_MASK) != 0UL;
    const bool sawResume =
        (causes & USBD_EVENTCAUSE_RESUME_MASK) != 0UL;

    // RESUME wins when both causes were captured. Entering LOWPOWER in that
    // state can disconnect the USBD clock after the host has already resumed.
    if (sawResume) {
        if (!wakeFromLowPower()) {
            failClosedPeripheral();
            return false;
        }
        enableInterrupts();
        suspended_ = false;
        ready_ = attached_ && hasVbus;
        return true;
    }

    if (sawSuspend) {
        // Close both sides of the classic suspend/resume race: check once
        // before LOWPOWER, then again immediately after the write. The cause is
        // left to relatch EVENTS_USBEVENT if it arrives after the second check.
        if ((reg32(USBD_BASE, EVENTCAUSE) &
             USBD_EVENTCAUSE_RESUME_MASK) == 0UL) {
            keepOnlyUsbdWakeInterrupt();
            reg32(USBD_BASE, LOWPOWER) = 1UL;
            const uint32_t lowPowerReadback = reg32(USBD_BASE, LOWPOWER);
            (void)lowPowerReadback;
        }

        if ((reg32(USBD_BASE, EVENTCAUSE) &
             USBD_EVENTCAUSE_RESUME_MASK) != 0UL) {
            reg32(USBD_BASE, EVENTCAUSE) = USBD_EVENTCAUSE_RESUME_MASK;
            reg32(USBD_BASE, EVENTS_USBEVENT) = 0UL;
            if (!wakeFromLowPower()) {
                failClosedPeripheral();
                return false;
            }
            enableInterrupts();
            suspended_ = false;
            ready_ = attached_ && hasVbus;
            return true;
        }

        suspended_ = true;
        return false;
    }

    return !suspended_;
}

void NrfUsbdDriver::processBusState(bool hasVbus) {
    // Nordic documents endpoint, SOF, and configuration registers as
    // inaccessible in LOWPOWER. Until RESUME has completed the protected wake,
    // inspect only the wake-capable aggregate event and EVENTCAUSE registers.
    if (suspended_) {
        (void)processUsbEvent(hasVbus);
        return;
    }

    if (reg32(USBD_BASE, EVENTS_USBRESET) != 0UL) {
        reg32(USBD_BASE, EVENTS_USBRESET) = 0UL;
        abortEasyDmaAfterBusReset();
        abortEp0Transfer();
        resetConnectionState();
    }

    if (reg32(USBD_BASE, EVENTS_STARTED) != 0UL) {
        reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
        started_ = true;
    }

    if (reg32(USBD_BASE, eventEndEpinOffset(0U)) != 0UL) {
        (void)completeEasyDma(DmaDirection::In, 0U);
        reg32(USBD_BASE, eventEndEpinOffset(0U)) = 0UL;
        completePendingAddress();
    }

    if (reg32(USBD_BASE, eventEndEpinOffset(SERVICE_NOTIFICATION_EP)) != 0UL) {
        (void)completeEasyDma(DmaDirection::In, SERVICE_NOTIFICATION_EP);
        reg32(USBD_BASE, eventEndEpinOffset(SERVICE_NOTIFICATION_EP)) = 0UL;
        notificationInFlight_ = false;
        diagResetAtUsbdBeginStage(21UL);
    }

    if (userPortEnabled() && reg32(USBD_BASE, eventEndEpinOffset(USER_NOTIFICATION_EP)) != 0UL) {
        (void)completeEasyDma(DmaDirection::In, USER_NOTIFICATION_EP);
        reg32(USBD_BASE, eventEndEpinOffset(USER_NOTIFICATION_EP)) = 0UL;
        userNotificationInFlight_ = false;
    }

    if (reg32(USBD_BASE, EVENTS_EP0DATADONE) != 0UL) {
        reg32(USBD_BASE, EVENTS_EP0DATADONE) = 0UL;
        if (pollTraceEnabled()) {
            ++g_usbdPollTrace.ep0DataDoneEvents;
        }
        // Route EP0DATADONE based on OUR tracked transfer state, NOT the volatile
        // BMREQUESTTYPE register. Windows can issue a follow-up setup (e.g.
        // GET_LINE_CODING immediately after SET_LINE_CODING) BEFORE the firmware
        // poll observes the OUT-data DONE event — that follow-up setup overwrites
        // BMREQUESTTYPE so the direction bit reads back as IN. The previous logic
        // then routed an OUT data completion into the IN-side branch and the
        // pending SET_LINE_CODING payload was dropped on the floor.
        const bool inXferActive =
            ep0InXferPhase_ == Ep0InXferPhase::Data ||
            ep0InXferPhase_ == Ep0InXferPhase::StatusPending;
        const bool outXferActive = pendingControlOut_ != ControlOutTransfer::None;
        if (inXferActive) {
            if (ep0InXferPhase_ == Ep0InXferPhase::StatusPending) {
                ep0InXferPhase_ = Ep0InXferPhase::Idle;
            } else if (ep0InXferPhase_ == Ep0InXferPhase::Data) {
                sendEp0ControlInChunkOrAdvanceStatus();
            }
        } else if (outXferActive) {
            // CRITICAL: EP0DATADONE only signals that the host-to-device data has been
            // accepted INTO THE PERIPHERAL'S INTERNAL BUFFER. It does NOT mean the
            // bytes are in our RAM controlOutBuffer_ yet. Per nRF52840 PS section
            // 6.36.6 (USBD EasyDMA model), the firmware must trigger TASKS_STARTEPOUT[0]
            // to EasyDMA the bytes out of the peripheral into PTR, and only then will
            // EVENTS_ENDEPOUT[0] (+ EPOUT[0].AMOUNT) reflect the actual byte count.
            // Without this step, AMOUNT stays 0, completeControlOutTransfer reads a
            // zero length, and the `if (controlOutLength_ >= 7U)` guard inside the
            // ServiceLineCoding case drops the entire SET_LINE_CODING payload — which
            // is precisely why the device's baud was stuck at its 115200 default and
            // the 1200 bps touch never armed.
            if (startEasyDma(DmaDirection::Out, 0U)) {
                bool completed = false;
                for (uint32_t spin = 0UL; spin < USBD_START_CAPTURE_TIMEOUT_SPINS; ++spin) {
                    if (reg32(USBD_BASE, eventEndEpoutOffset(0U)) != 0UL) {
                        (void)completeEasyDma(DmaDirection::Out, 0U);
                        reg32(USBD_BASE, eventEndEpoutOffset(0U)) = 0UL;
                        completed = true;
                        break;
                    }
                }
                if (completed) {
                    completeControlOutTransfer();
                    completePendingAddress();
                }
            }
        }
        // Else: spurious EP0DATADONE with neither side tracked — drop silently.
    }

    // Nordic's USBD driver intentionally handles a newly latched SETUP after
    // every completion that was already pending in the same IRQ snapshot. A
    // host may issue the next SETUP immediately after the preceding status
    // stage. Handling SETUP first would make abortEp0Transfer() erase that
    // preceding EP0DATADONE/END event and can strand EP0 during enumeration.
    // The fresh SETUP still aborts any genuinely unfinished control transfer.
    if (reg32(USBD_BASE, EVENTS_EP0SETUP) != 0UL) {
        reg32(USBD_BASE, EVENTS_EP0SETUP) = 0UL;
        abortEp0Transfer();
        if (pollTraceEnabled()) {
            ++g_usbdPollTrace.ep0SetupEvents;
        }
        diagResetAtUsbdBeginStage(7UL);
        serviceSetup();
        diagResetAtUsbdBeginStage(8UL);
    }

    // A host OUT packet has been buffered when its EPDATASTATUS OUT bit is set
    // (EPOUT1..7 = bits 17..23); for each we trigger an EasyDMA STARTEPOUT
    // (queueDataOut), then ENDEPOUT fires and the branch below copies the bytes
    // into the rx ring.
    //
    // We drain straight from EPDATASTATUS rather than gating on EVENTS_EPDATA.
    // EVENTS_EPDATA is only an aggregate "a data endpoint advanced" wake. The
    // GDB stub services USB by busy-polling this handler (USBD IRQ masked while
    // halted) thousands of times faster than the ISR would, and that polling
    // can sample EVENTS_EPDATA in the brief window *before* the per-endpoint
    // OUT bit latches in EPDATASTATUS — or after a prior pass already cleared
    // the aggregate event. Gating the drain on EVENTS_EPDATA then strands the
    // received packet in the endpoint's internal buffer with its EPDATASTATUS
    // bit stuck set, so every subsequent OUT is NAK'd and host writes time out.
    // (This never bit the normal USBD ISR because interrupt latency lets
    // EPDATASTATUS latch before the handler reads it.) Reading EPDATASTATUS
    // directly each pass recovers any such packet on the very next poll. This
    // was the real cause of the "service CDC OUT NAKs while the stub is halted"
    // bug — the GDB stub is the first feature to receive bulk OUT on the
    // service CDC, so the latent busy-poll race finally surfaced.
    // EPDATA fires when a data endpoint advances. Use it for the IN-side ACK:
    // EPDATASTATUS.EPIN<n> sets when the HOST has read an IN packet. Clear the
    // in-flight flag on THAT, not on EVENTS_ENDEPIN below — ENDEPIN only means
    // the buffer->FIFO DMA finished; the packet still sits in the endpoint FIFO
    // until the host reads it, and STARTEPIN'ing the next chunk before then
    // overwrites the unread FIFO and drops the chunk. That silently truncated
    // multi-packet IN replies (e.g. the GDB qSupported feature list lost its
    // first 64-byte chunk, leaving "$+#ad"). Clear only the IN bits here; the
    // OUT bits (EPOUT2/EPOUT4) belong to drainServiceDataOut().
    if (reg32(USBD_BASE, EVENTS_EPDATA) != 0UL) {
        reg32(USBD_BASE, EVENTS_EPDATA) = 0UL;
        const uint32_t eds = reg32(USBD_BASE, EPDATASTATUS);
        const uint32_t serviceInBit = 1UL << SERVICE_DATA_EP; // EPIN2 = bit 2
        const uint32_t userInBit = 1UL << USER_DATA_EP;       // EPIN4 = bit 4
        uint32_t inAck = 0UL;
        if ((eds & serviceInBit) != 0UL) {
            dataInFlight_ = false;
            inAck |= serviceInBit;
        }
        if ((eds & userInBit) != 0UL) {
            userDataInFlight_ = false;
            inAck |= userInBit;
        }
        if (inAck != 0UL) {
            reg32(USBD_BASE, EPDATASTATUS) = inAck; // clear handled IN bits (W1C)
        }
        // OUT bits are deliberately NOT drained here. Draining in the EPDATA
        // ISR samples EPDATASTATUS precisely in the IN-ack window, where this
        // hardware can transiently set OUT bits with no real packet; each phantom fetch
        // then replays stale FIFO bytes, and because the consumer's response
        // TX causes the next IN-ack, the replays become a self-sustaining
        // storm that can starve the sketch loop. The foreground pumpRx()
        // (Serial.available()/read()) is the sole OUT consumer; it samples at
        // uncorrelated times, which is the same timing the GDB stub's poll
        // loop has always used successfully.
    }

    if (reg32(USBD_BASE, eventEndEpoutOffset(SERVICE_DATA_EP)) != 0UL) {
        // Clear-only: this branch served the dead queueDataOut() model. Real
        // packets are consumed via their EPDATASTATUS bit (drainServiceDataOut
        // below / pumpRx); a bare ENDEPOUT on an armed endpoint is a stale
        // speculative re-delivery and must NOT be pushed into the rx ring.
        (void)completeEasyDma(DmaDirection::Out, SERVICE_DATA_EP);
        reg32(USBD_BASE, eventEndEpoutOffset(SERVICE_DATA_EP)) = 0UL;
    }

    if (reg32(USBD_BASE, eventEndEpinOffset(SERVICE_DATA_EP)) != 0UL) {
        // DMA buffer->FIFO done; the host-read ACK (EPDATASTATUS.EPIN2 above)
        // is what frees us to send the next chunk, so don't clear dataInFlight_.
        (void)completeEasyDma(DmaDirection::In, SERVICE_DATA_EP);
        reg32(USBD_BASE, eventEndEpinOffset(SERVICE_DATA_EP)) = 0UL;
        diagResetAtUsbdBeginStage(18UL);
    }

    if (userPortEnabled() && reg32(USBD_BASE, eventEndEpoutOffset(USER_DATA_EP)) != 0UL) {
        (void)completeEasyDma(DmaDirection::Out, USER_DATA_EP);
        reg32(USBD_BASE, eventEndEpoutOffset(USER_DATA_EP)) = 0UL; // clear-only, see above
    }

    if (userPortEnabled() && reg32(USBD_BASE, eventEndEpinOffset(USER_DATA_EP)) != 0UL) {
        (void)completeEasyDma(DmaDirection::In, USER_DATA_EP);
        reg32(USBD_BASE, eventEndEpinOffset(USER_DATA_EP)) = 0UL;
    }

    for (uint8_t endpoint = firstDynamicEndpoint(); endpoint < USB_MAX_ENDPOINTS; ++endpoint) {
        if (reg32(USBD_BASE, eventEndEpinOffset(endpoint)) != 0UL) {
            (void)completeEasyDma(DmaDirection::In, endpoint);
            reg32(USBD_BASE, eventEndEpinOffset(endpoint)) = 0UL;
            dynamicInBusy_[endpoint] = false;
            dynamicInLengths_[endpoint] = 0U;
            PluggableUSB().endpointInComplete(endpoint);
        }
    }

    (void)processUsbEvent(hasVbus);
}

bool NrfUsbdDriver::enabled() const {
    return enabled_;
}

bool NrfUsbdDriver::attached() const {
    return enabled_ && attached_;
}

bool NrfUsbdDriver::ready() const {
    return attached_ && ready_;
}

bool NrfUsbdDriver::connected() const {
    return enabled_ && attached_ && ready_ && configured_ && !suspended_ && vbusDetected() && dtr_;
}

bool NrfUsbdDriver::userConnected() const {
    return enabled_ && attached_ && ready_ && configured_ && !suspended_ && vbusDetected() && userPortEnabled() && userDtr_;
}

bool NrfUsbdDriver::configured() const {
    return attached_ && configured_;
}

bool NrfUsbdDriver::suspended() const {
    return attached_ && suspended_;
}

bool NrfUsbdDriver::dtr() const {
    return dtr_;
}

bool NrfUsbdDriver::rts() const {
    return rts_;
}

bool NrfUsbdDriver::userDtr() const {
    return userDtr_;
}

bool NrfUsbdDriver::userRts() const {
    return userRts_;
}

unsigned long NrfUsbdDriver::baud() const {
    return lineCoding_.baudRate;
}

unsigned long NrfUsbdDriver::userBaud() const {
    return userLineCoding_.baudRate;
}

uint8_t NrfUsbdDriver::address() const {
    return address_;
}

uint8_t NrfUsbdDriver::configuration() const {
    return configuration_;
}

const NrfUsbLineCoding &NrfUsbdDriver::lineCoding() const {
    return lineCoding_;
}

const NrfUsbLineCoding &NrfUsbdDriver::userLineCoding() const {
    return userLineCoding_;
}

int NrfUsbdDriver::available() const {
    return static_cast<int>(ringPending(rxHead_, rxTail_));
}

int NrfUsbdDriver::peek() const {
    return ringPeekRx();
}

int NrfUsbdDriver::read() {
    return ringPopRx();
}

size_t NrfUsbdDriver::write(uint8_t value) {
    if (!ringPushTx(value)) {
        return 0U;
    }
    if (configured_ && !suspended_) {
        UsbdIrqLock lock;
        serviceDataIn(false);
    }
    return 1U;
}

void NrfUsbdDriver::kickServiceDataIn() {
    if (!enabled_ || !configured_ || suspended_) {
        return;
    }
    UsbdIrqLock lock;
    serviceDataIn(false);
    serviceNotificationIn(false);
}

void NrfUsbdDriver::pumpRx() {
    if (!enabled_ || !configured_ || suspended_) {
        return;
    }
    UsbdIrqLock lock;

    // This silicon NAKs every host OUT until a first STARTEPOUT arms the
    // endpoint, and armCdcDataOut() is unsafe during enumeration — so arm
    // lazily here, on the first post-configuration foreground pump (the GDB
    // stub does the same on takeover). The arming STARTEPOUT can pull stale
    // FIFO content whose ENDEPOUT lands after armCdcDataOut's own discard
    // window — drop it here too and return, so it can never be harvested as
    // host data on this pass.
    if (!cdcOutArmed_) {
        armCdcDataOut();
        for (uint32_t spin = 0UL; spin < (USBD_DRAIN_OUT_SPINS * 4UL); ++spin) {
            if (reg32(USBD_BASE, eventEndEpoutOffset(SERVICE_DATA_EP)) != 0UL) {
                (void)completeEasyDma(DmaDirection::Out, SERVICE_DATA_EP);
                reg32(USBD_BASE, eventEndEpoutOffset(SERVICE_DATA_EP)) = 0UL;
            }
            if (userPortEnabled() && reg32(USBD_BASE, eventEndEpoutOffset(USER_DATA_EP)) != 0UL) {
                (void)completeEasyDma(DmaDirection::Out, USER_DATA_EP);
                reg32(USBD_BASE, eventEndEpoutOffset(USER_DATA_EP)) = 0UL;
            }
        }
        cdcOutArmed_ = true;
        return;
    }

    // Real host packets are signalled ONLY by their EPDATASTATUS bit (the
    // GDB-stub-proven reception model). A bare ENDEPOUT on an armed endpoint
    // is a speculative DMA completion re-delivering stale FIFO content —
    // clear it WITHOUT servicing, or the same dead bytes get pushed into the
    // rx ring after every real packet.
    if (reg32(USBD_BASE, eventEndEpoutOffset(SERVICE_DATA_EP)) != 0UL) {
        (void)completeEasyDma(DmaDirection::Out, SERVICE_DATA_EP);
        reg32(USBD_BASE, eventEndEpoutOffset(SERVICE_DATA_EP)) = 0UL;
    }
    if (userPortEnabled() && reg32(USBD_BASE, eventEndEpoutOffset(USER_DATA_EP)) != 0UL) {
        (void)completeEasyDma(DmaDirection::Out, USER_DATA_EP);
        reg32(USBD_BASE, eventEndEpoutOffset(USER_DATA_EP)) = 0UL;
    }

    drainServiceDataOut();
}

bool NrfUsbdDriver::sendInPacket(uint8_t endpoint, const void *data, size_t length) {
    if (!enabled_ || !attached_ || !configured_ || suspended_ || data == nullptr) {
        return false;
    }
    if (endpoint < firstDynamicEndpoint() || endpoint >= USB_MAX_ENDPOINTS) {
        return false;
    }

    // Hold the busy test-and-set plus the EasyDMA kick atomic against the ISR,
    // which clears dynamicInBusy_[endpoint] on ENDEPIN completion.
    UsbdIrqLock lock;
    if (dynamicInBusy_[endpoint] || dmaDirection_ != DmaDirection::None) {
        return false;
    }

    size_t actualLength = length;
    if (actualLength > DATA_EP_MAX_PACKET) {
        actualLength = DATA_EP_MAX_PACKET;
    }
    const uint8_t *source = reinterpret_cast<const uint8_t *>(data);
    for (size_t index = 0; index < actualLength; ++index) {
        dynamicInBuffers_[endpoint][index] = source[index];
    }
    dynamicInLengths_[endpoint] = static_cast<uint8_t>(actualLength);
    reg32(USBD_BASE, EPINEN) |= endpointMask(endpoint);
    reg32(USBD_BASE, epinPtrOffset(endpoint)) = reinterpret_cast<uint32_t>(&dynamicInBuffers_[endpoint][0]);
    reg32(USBD_BASE, epinMaxcntOffset(endpoint)) = static_cast<uint32_t>(actualLength);
    if (!startEasyDma(DmaDirection::In, endpoint)) {
        dynamicInLengths_[endpoint] = 0U;
        return false;
    }
    dynamicInBusy_[endpoint] = true;
    return true;
}

void NrfUsbdDriver::flush() {
    if (!enabled_ || !configured_ || suspended_) {
        return;
    }

    // The drain loop calls processBusState()/serviceDataIn() itself, so keep the
    // whole pass mutually exclusive with the ISR; events that arrive meanwhile
    // latch and are taken the moment we return and unmask.
    UsbdIrqLock lock;

    if (stubHalted_) {
        // While the GDB stub is halted SysTick is stopped, so millis() is
        // FROZEN — the normal millis()-based 50 ms bound below never trips and
        // flush() would spin for tens of seconds (gdb then times out before the
        // reply lands). Bound the drain with a spin counter instead, pumping the
        // OUT/IN service path (processBusState advances IN ENDEPIN, serviceDataIn
        // starts the next chunk) until the tx ring is empty or the link drops.
        for (uint32_t spin = 0UL; spin < USBD_STUB_FLUSH_SPINS; ++spin) {
            if ((txPending() == 0U && !dataInFlight_) || !enabled_) {
                break;
            }
            if (!configured_ || suspended_ || !vbusDetected()) {
                break;
            }
            processBusState(vbusDetected());
            if (!configured_ || suspended_ || !vbusDetected()) {
                break;
            }
            serviceDataIn(false);
        }
        return;
    }

    const uint32_t start = millis();
    while ((txPending() != 0U || dataInFlight_) && enabled_) {
        processBusState(vbusDetected());
        if (!configured_ || suspended_ || !vbusDetected()) {
            break;
        }
        serviceDataIn(false);
        if ((millis() - start) >= 50UL) {
            break;
        }
    }
}

void NrfUsbdDriver::injectRx(const uint8_t *data, size_t length) {
    if (data == nullptr) {
        return;
    }

    // serviceDataOut() (ISR) is the other producer into the service RX ring;
    // serialize so the two producers never interleave a head update.
    UsbdIrqLock lock;
    for (size_t index = 0; index < length; ++index) {
        ringPushRx(data[index]);
    }
}

void NrfUsbdDriver::setLineCoding(const NrfUsbLineCoding &lineCoding) {
    lineCoding_ = lineCoding;
}

void NrfUsbdDriver::setLineState(bool dtr, bool rts) {
    dtr_ = dtr;
    rts_ = rts;
}

int NrfUsbdDriver::userAvailable() const {
    return static_cast<int>(ringPending(userRxHead_, userRxTail_));
}

int NrfUsbdDriver::userPeek() const {
    return userRingPeekRx();
}

int NrfUsbdDriver::userRead() {
    return userRingPopRx();
}

size_t NrfUsbdDriver::userWrite(uint8_t value) {
    if (!userPortEnabled() || !userRingPushTx(value)) {
        return 0U;
    }
    if (configured_ && !suspended_) {
        UsbdIrqLock lock;
        serviceDataIn(true);
    }
    return 1U;
}

size_t NrfUsbdDriver::userWrite(const uint8_t *data, size_t length) {
    if (!userPortEnabled() || data == nullptr) {
        return 0U;
    }
    // SPSC ring: the foreground is the sole producer, the ISR drains (consumer),
    // so userRingPushTx() is safe lock-free here (same as the per-byte path).
    size_t written = 0U;
    while (written < length && userRingPushTx(data[written])) {
        ++written;
    }
    if (written > 0U && configured_ && !suspended_) {
        UsbdIrqLock lock;
        serviceDataIn(true);  // arm once for the whole block (was once per byte)
    }
    return written;
}

void NrfUsbdDriver::userFlush() {
    if (!userPortEnabled() || !enabled_ || !configured_ || suspended_) {
        return;
    }

    UsbdIrqLock lock;
    const uint32_t start = millis();
    while ((userTxPending() != 0U || userDataInFlight_) && enabled_) {
        processBusState(vbusDetected());
        if (!configured_ || suspended_ || !vbusDetected()) {
            break;
        }
        serviceDataIn(true);
        if ((millis() - start) >= 50UL) {
            break;
        }
    }
}

void NrfUsbdDriver::setUserLineCoding(const NrfUsbLineCoding &lineCoding) {
    userLineCoding_ = lineCoding;
}

void NrfUsbdDriver::setUserLineState(bool dtr, bool rts) {
    userDtr_ = dtr;
    userRts_ = rts;
}

NrfUsbdStatus NrfUsbdDriver::status() const {
    return {enabled_, started_, attached_, vbusDetected(), ready_, configured_, suspended_, cdcActive_, address_, configuration_, eventCause_};
}

void NrfUsbdDriver::resetConnectionState() {
    // NOTE: do NOT clear ready_ here. ready_ reflects the USBD *peripheral*
    // readiness after ENABLE (the one-shot EVENTCAUSE.READY observed in begin()),
    // not the USB *bus* connection. resetConnectionState() runs on every USB bus
    // reset (which the host issues during normal enumeration); clearing ready_
    // there left it false forever - begin() consumes the READY event, so the
    // processBusState path that sets ready_ never fires again - so
    // USBDevice.connected() never returned true and any sketch that waits on it
    // (e.g. the GDB-stub debug example) hung before reaching the breakpoint.
    // ready_ is set only after begin() completes the full power handshake and
    // is cleared by end() or any fail-closed disconnect/retry path.
    configured_ = false;
    suspended_ = false;
    cdcActive_ = false;
    dataInFlight_ = false;
    notificationInFlight_ = false;
    notificationPending_ = false;
    userDataInFlight_ = false;
    userNotificationInFlight_ = false;
    userNotificationPending_ = false;
    dtr_ = false;
    rts_ = false;
    userDtr_ = false;
    userRts_ = false;
    pendingAddressValid_ = false;
    pendingControlOut_ = ControlOutTransfer::None;
    controlOutExpected_ = 0U;
    controlOutLength_ = 0U;
    detachRequestMagic_ = 0UL;
    detachCause_ = 0UL;
    resetEp0InXferState();
    serviceSawNonResetBaud_ = false;
    serviceTouchPending_ = false;
    serviceTouchResetMillis_ = 0UL;
    ignoredResetTouchCount_ = 0U;
    address_ = 0U;
    pendingAddress_ = 0U;
    configuration_ = 0U;
    configStartMillis_ = (enabled_ && attached_) ? millis() : 0UL;
    configuredMillis_ = 0UL;
    serialStateBitmap_ = 0U;
    userSerialStateBitmap_ = 0U;
    if (enabled_) {
        reg32(USBD_BASE, EPINEN) = endpointMask(0U);
        reg32(USBD_BASE, EPOUTEN) = endpointMask(0U);
    }
    resetDynamicEndpoints();
}

void NrfUsbdDriver::resetDynamicEndpoints() {
    for (uint8_t endpoint = 0U; endpoint < USB_MAX_ENDPOINTS; ++endpoint) {
        dynamicInLengths_[endpoint] = 0U;
        dynamicInBusy_[endpoint] = false;
    }
}

void NrfUsbdDriver::initDescriptors() {
    const NrfSystemProfile &profile = nrfSystemProfile();
    const uint8_t deviceDescriptor[] = {
        18U, USB_DESC_DEVICE,
        0x00U, 0x02U,
        0xEFU, 0x02U, 0x01U,
        static_cast<uint8_t>(CONTROL_EP_MAX_PACKET),
        static_cast<uint8_t>(profile.usbVid & 0xFFU), static_cast<uint8_t>((profile.usbVid >> 8U) & 0xFFU),
        static_cast<uint8_t>(profile.usbPid & 0xFFU), static_cast<uint8_t>((profile.usbPid >> 8U) & 0xFFU),
        0x00U, 0x01U,
        0x01U, 0x02U, 0x03U,
        0x01U
    };

    for (size_t index = 0; index < sizeof(deviceDescriptor); ++index) {
        deviceDescriptor_[index] = deviceDescriptor[index];
    }

    configurationDescriptorLength_ = 0U;
    const auto appendDescriptor = [&](const uint8_t *data, size_t length) {
        for (size_t index = 0; index < length && configurationDescriptorLength_ < sizeof(configurationDescriptor_); ++index) {
            configurationDescriptor_[configurationDescriptorLength_++] = data[index];
        }
    };
    const auto appendCdcDescriptor =
        [&](uint8_t controlInterface,
            uint8_t dataInterface,
            uint8_t notificationEndpoint,
            uint8_t dataEndpoint,
            uint8_t controlStringIndex,
            uint8_t dataStringIndex) {
        const uint8_t descriptor[] = {
            0x08U, USB_DESC_IAD, controlInterface, 0x02U, 0x02U, 0x02U, 0x01U, controlStringIndex,
            0x09U, USB_DESC_INTERFACE, controlInterface, 0x00U, 0x01U, 0x02U, 0x02U, 0x01U, controlStringIndex,
            0x05U, USB_DESC_CS_INTERFACE, 0x00U, 0x10U, 0x01U,
            0x05U, USB_DESC_CS_INTERFACE, 0x01U, 0x00U, dataInterface,
            0x04U, USB_DESC_CS_INTERFACE, 0x02U, 0x02U,
            0x05U, USB_DESC_CS_INTERFACE, 0x06U, controlInterface, dataInterface,
            0x07U, USB_DESC_ENDPOINT, epAddressIn(notificationEndpoint), 0x03U, 0x10U, 0x00U, 0x10U,
            0x09U, USB_DESC_INTERFACE, dataInterface, 0x00U, 0x02U, 0x0AU, 0x00U, 0x00U, dataStringIndex,
            0x07U, USB_DESC_ENDPOINT, epAddressOut(dataEndpoint), 0x02U, 0x40U, 0x00U, 0x00U,
            0x07U, USB_DESC_ENDPOINT, epAddressIn(dataEndpoint), 0x02U, 0x40U, 0x00U, 0x00U,
        };
        appendDescriptor(descriptor, sizeof(descriptor));
    };
    const uint8_t configHeader[] = {
        0x09U, USB_DESC_CONFIGURATION, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U, 0x80U, 0x32U,
    };
    appendDescriptor(configHeader, sizeof(configHeader));
    appendCdcDescriptor(SERVICE_CONTROL_INTERFACE,
                        SERVICE_DATA_INTERFACE,
                        SERVICE_NOTIFICATION_EP,
                        SERVICE_DATA_EP,
                        USB_STRING_SERVICE_CONTROL,
                        USB_STRING_SERVICE_DATA);
    uint8_t interfaceCount = 2U;
    if (userPortEnabled()) {
        appendCdcDescriptor(USER_CONTROL_INTERFACE,
                            USER_DATA_INTERFACE,
                            USER_NOTIFICATION_EP,
                            USER_DATA_EP,
                            USB_STRING_USER_CONTROL,
                            USB_STRING_USER_DATA);
        interfaceCount = 4U;
    }
#if !defined(NRF_USB_RUNTIME_DISABLE_APP_DFU) || (NRF_USB_RUNTIME_DISABLE_APP_DFU == 0)
    const uint8_t dfuDescriptor[] = {
        0x09U, USB_DESC_INTERFACE, dfuInterfaceNumber(), 0x00U, 0x00U, 0xFEU, 0x01U, 0x01U, USB_STRING_DFU,
        0x09U, USB_DESC_DFU_FUNCTIONAL, 0x0DU, 0xFFU, 0x00U, 0x00U, 0x08U, 0x1AU, 0x01U,
    };
    appendDescriptor(dfuDescriptor, sizeof(dfuDescriptor));
    ++interfaceCount;
#endif

    PluggableUSB().beginDescriptorBuild(configurationDescriptor_, sizeof(configurationDescriptor_), configurationDescriptorLength_);
    configurationDescriptorLength_ = static_cast<uint16_t>(configurationDescriptorLength_ + PluggableUSB().getInterface(&interfaceCount));
    configurationDescriptorLength_ = static_cast<uint16_t>(PluggableUSB().endDescriptorBuild());

    configurationDescriptor_[2] = static_cast<uint8_t>(configurationDescriptorLength_ & 0xFFU);
    configurationDescriptor_[3] = static_cast<uint8_t>((configurationDescriptorLength_ >> 8U) & 0xFFU);
    configurationDescriptor_[4] = interfaceCount;
}

void NrfUsbdDriver::clearEvents() {
    reg32(USBD_BASE, EVENTS_USBRESET) = 0UL;
    reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
    for (uint8_t endpoint = 0; endpoint < 8U; ++endpoint) {
        reg32(USBD_BASE, eventEndEpinOffset(endpoint)) = 0UL;
        reg32(USBD_BASE, eventEndEpoutOffset(endpoint)) = 0UL;
    }
    reg32(USBD_BASE, EVENTS_EP0DATADONE) = 0UL;
    reg32(USBD_BASE, EVENTS_ENDISOIN) = 0UL;
    reg32(USBD_BASE, EVENTS_ENDISOOUT) = 0UL;
    reg32(USBD_BASE, EVENTS_SOF) = 0UL;
    reg32(USBD_BASE, EVENTS_USBEVENT) = 0UL;
    reg32(USBD_BASE, EVENTS_EP0SETUP) = 0UL;
    reg32(USBD_BASE, EVENTS_EPDATA) = 0UL;
    reg32(USBD_BASE, EVENTCAUSE) = 0xFFFFFFFFUL;
}

void NrfUsbdDriver::enableInterrupts() {
#if defined(NRF_USBD_POLL_ONLY) && (NRF_USBD_POLL_ONLY == 1)
    return;
#endif
    const uint32_t interruptMask =
        USBD_INT_USBRESET_MASK |
        USBD_INT_STARTED_MASK |
        USBD_INT_ENDEPIN0_MASK |
        USBD_INT_ENDEPIN1_MASK |
        USBD_INT_ENDEPIN2_MASK |
        USBD_INT_ENDEPIN3_MASK |
        USBD_INT_ENDEPIN4_MASK |
        USBD_INT_ENDEPIN5_MASK |
        USBD_INT_ENDEPIN6_MASK |
        USBD_INT_ENDEPIN7_MASK |
        USBD_INT_EP0DATADONE_MASK |
        USBD_INT_ENDEPOUT2_MASK |
        USBD_INT_ENDEPOUT4_MASK |
        USBD_INT_USBEVENT_MASK |
        USBD_INT_EP0SETUP_MASK |
        USBD_INT_EPDATA_MASK;
    reg32(USBD_BASE, INTENSET) = interruptMask;
    // Run USB at a low preemption priority (6 of 0..7) so genuinely
    // time-critical ISRs -- the BLE radio/timer chain in particular -- always
    // win the arbitration. USB enumeration tolerates the small added latency,
    // and the foreground critical sections (UsbdIrqLock) keep it race-free.
    setNvicPriority(USBD_IRQ_NUMBER, 6U);
    enableNvicIrq(USBD_IRQ_NUMBER);
    if (deferredUsbCauses_ != 0UL) {
        // The aggregate event may have been consumed while wake acknowledgement
        // captured a concurrent cause. Deliver the software-deferred cause even
        // to sketches that never call poll()/yield().
        setPendingNvicIrq(USBD_IRQ_NUMBER);
    }
}

void NrfUsbdDriver::disableInterrupts() {
    // Clear every source, including any bootloader-enabled ISO/SOF source that
    // this stack does not otherwise use, then remove a latched NVIC request.
    reg32(USBD_BASE, INTENCLR) = 0xFFFFFFFFUL;
    disableNvicIrq(USBD_IRQ_NUMBER);
    clearPendingNvicIrq(USBD_IRQ_NUMBER);
}

void NrfUsbdDriver::enablePullup(bool enabled) {
    // Never turn the physical D+ pull-up on from a board-policy assumption.
    // The peripheral must have completed READY + OUTPUTRDY and the POWER block
    // must still report actual VBUS at the final register write.
    if (enabled && enabled_ && ready_ && vbusDetected() && usbPwrRdy()) {
        reg32(USBD_BASE, USBPULLUP) = 1UL;
    } else {
        reg32(USBD_BASE, USBPULLUP) = 0UL;
    }
}

void NrfUsbdDriver::startCdcEndpoints() {
    reg32(USBD_BASE, EPINEN) |= endpointMask(SERVICE_NOTIFICATION_EP) | endpointMask(SERVICE_DATA_EP);
    reg32(USBD_BASE, EPOUTEN) |= endpointMask(SERVICE_DATA_EP);
    resetEndpointDataState(epAddressIn(SERVICE_NOTIFICATION_EP));
    resetEndpointDataState(epAddressIn(SERVICE_DATA_EP));
    resetEndpointDataState(epAddressOut(SERVICE_DATA_EP));
    // Do NOT pre-trigger TASKS_STARTEPOUT here: with no data yet in the internal
    // buffer it wedges the endpoint (and during enumeration it breaks config).
    if (userPortEnabled()) {
        reg32(USBD_BASE, EPINEN) |= endpointMask(USER_NOTIFICATION_EP) | endpointMask(USER_DATA_EP);
        reg32(USBD_BASE, EPOUTEN) |= endpointMask(USER_DATA_EP);
        resetEndpointDataState(epAddressIn(USER_NOTIFICATION_EP));
        resetEndpointDataState(epAddressIn(USER_DATA_EP));
        resetEndpointDataState(epAddressOut(USER_DATA_EP));
    }
    cdcActive_   = true;
    cdcOutArmed_ = false; // re-arm lazily from the first foreground pumpRx()
}

// Arm the OUT endpoint's EasyDMA to receive the next host packet directly into
// our RAM buffer. On affected hardware the "auto internal-buffer +
// EPDATASTATUS" reception model does not signal (a received packet leaves
// EVENTS_EPDATA and EPDATASTATUS untouched), so explicitly STARTEPOUT arms reception;
// the host's data then EasyDMA's into the buffer and EVENTS_ENDEPOUT fires.
// Asynchronous: ENDEPOUT is serviced (serviceDataOut) and re-armed by the
// EVENTS_ENDEPOUT handler.
void NrfUsbdDriver::queueDataOut(bool userPort) {
    const uint8_t endpoint = userPort ? USER_DATA_EP : SERVICE_DATA_EP;
    uint8_t *buffer = userPort ? &userEndpointOutBuffer_[0] : &endpointOutBuffer_[0];
    reg32(USBD_BASE, epoutPtrOffset(endpoint)) = reinterpret_cast<uint32_t>(buffer);
    reg32(USBD_BASE, epoutMaxcntOffset(endpoint)) = DATA_EP_MAX_PACKET;
    (void)startEasyDma(DmaDirection::Out, endpoint);
}

void NrfUsbdDriver::armCdcDataOut() {
    // Prime the OUT endpoints once: this silicon won't ACK a host OUT (the
    // data-pending bit never sets) until a STARTEPOUT has armed the endpoint.
    // After this single arm, each fetchOutPacket()'s STARTEPOUT re-arms for the
    // following packet, so no continuous/speculative STARTEPOUT is needed (which
    // would race an arriving packet and re-deliver stale FIFO content). Discard
    // whatever this initial STARTEPOUT pulls (stale/empty) and clear the bit.
    const uint8_t eps[2] = { SERVICE_DATA_EP, USER_DATA_EP };
    for (uint32_t k = 0U; k < 2U; ++k) {
        if (k == 1U && !userPortEnabled()) {
            break;
        }
        const uint8_t endpoint = eps[k];
        uint8_t *buffer = (endpoint == USER_DATA_EP) ? &userEndpointOutBuffer_[0] : &endpointOutBuffer_[0];
        reg32(USBD_BASE, epoutPtrOffset(endpoint)) = reinterpret_cast<uint32_t>(buffer);
        reg32(USBD_BASE, epoutMaxcntOffset(endpoint)) = DATA_EP_MAX_PACKET;
        if (!startEasyDma(DmaDirection::Out, endpoint)) {
            return;
        }
        for (uint32_t spin = 0UL; spin < USBD_DRAIN_OUT_SPINS; ++spin) {
            if (reg32(USBD_BASE, eventEndEpoutOffset(endpoint)) != 0UL) {
                (void)completeEasyDma(DmaDirection::Out, endpoint);
                reg32(USBD_BASE, eventEndEpoutOffset(endpoint)) = 0UL;
                break;
            }
        }
        reg32(USBD_BASE, EPDATASTATUS) =
            1UL << (EPDATASTATUS_OUT_BASE_BIT + endpoint - 1U); // discard/ack
        reg32(USBD_BASE, sizeEpoutOffset(endpoint)) = 0UL; // buffer-consumed handshake
    }
}

void NrfUsbdDriver::fetchOutPacket(uint8_t endpoint, bool userPort, uint32_t statusBit) {
    // Called only when EPDATASTATUS.<statusBit> is already set: a COMPLETE host
    // packet is in this endpoint's FIFO (USB OUT is atomic). STARTEPOUT pulls it
    // into RAM AND re-arms the endpoint for the next OUT; we then consume the
    // data bit (W1C) and hand the bytes to the rx ring. Reading the bit BEFORE
    // (in drainServiceDataOut) avoids the race where a speculative STARTEPOUT on
    // an idle endpoint re-delivers stale FIFO content and shifts the RSP stream.
    uint8_t *buffer = userPort ? &userEndpointOutBuffer_[0] : &endpointOutBuffer_[0];
    reg32(USBD_BASE, epoutPtrOffset(endpoint)) = reinterpret_cast<uint32_t>(buffer);
    reg32(USBD_BASE, epoutMaxcntOffset(endpoint)) = DATA_EP_MAX_PACKET;
    if (!startEasyDma(DmaDirection::Out, endpoint)) {
        return;
    }
    for (uint32_t spin = 0UL; spin < USBD_DRAIN_OUT_SPINS; ++spin) {
        if (reg32(USBD_BASE, eventEndEpoutOffset(endpoint)) != 0UL) {
            (void)completeEasyDma(DmaDirection::Out, endpoint);
            reg32(USBD_BASE, eventEndEpoutOffset(endpoint)) = 0UL;
            // Real packet length: SIZE.EPOUT[n] (read BEFORE the consume
            // handshake write below clears it). EPOUT[n].AMOUNT lies on this
            // affected hardware - it echoes MAXCNT (64) for every packet, which used to
            // deliver every short host packet padded with stale buffer bytes
            // (the long-standing "phantom replay" junk).
            const uint32_t amount = reg32(USBD_BASE, sizeEpoutOffset(endpoint));
            reg32(USBD_BASE, EPDATASTATUS) = statusBit; // consume/ACK (W1C)
            reg32(USBD_BASE, sizeEpoutOffset(endpoint)) = 0UL; // buffer-consumed handshake
            if (amount != 0UL && amount <= DATA_EP_MAX_PACKET) {
                serviceDataOut(userPort, amount);
            }
            return;
        }
    }
}

void NrfUsbdDriver::drainServiceDataOut() {
    if (!enabled_) {
        return;
    }
    // The nRF52840 USBD has a SINGLE shared EasyDMA: only one STARTEPIN/
    // STARTEPOUT may be in flight at a time. A STARTEPOUT here would collide
    // with the stub streaming an IN reply, corrupting it. Skip while any IN DMA
    // is pending; the stub re-pumps us right after the IN completes.
    if (dmaDirection_ != DmaDirection::None) {
        return;
    }
    // Bit-gated: STARTEPOUT only an endpoint whose OUT data bit is already set
    // (a whole packet arrived). The fetch re-arms for the next packet, so the
    // endpoint stays ready without a racy speculative re-arm. Both CDCs that
    // share this cable — service (GDB) and user (sketch Serial) — are handled so
    // a serial monitor on the user port can't wedge OUT while halted.
    const uint32_t eds = reg32(USBD_BASE, EPDATASTATUS);
    const uint32_t serviceOutBit = 1UL << (EPDATASTATUS_OUT_BASE_BIT + SERVICE_DATA_EP - 1U);
    const uint32_t userOutBit = 1UL << (EPDATASTATUS_OUT_BASE_BIT + USER_DATA_EP - 1U);
    if ((eds & serviceOutBit) != 0UL) {
        fetchOutPacket(SERVICE_DATA_EP, false, serviceOutBit);
    }
    if (userPortEnabled() && (eds & userOutBit) != 0UL) {
        fetchOutPacket(USER_DATA_EP, true, userOutBit);
    }
}

void NrfUsbdDriver::serviceDataOut(bool userPort, uint32_t received) {
    uint8_t *buffer = userPort ? &userEndpointOutBuffer_[0] : &endpointOutBuffer_[0];
    // NOTE: the caller passes the packet length read from SIZE.EPOUT[n]. The
    // EPOUT[n].AMOUNT register is not usable for this on affected hardware:
    // it reports MAXCNT (64) regardless of the real packet size, which made
    // every short host packet arrive padded with stale buffer bytes (the
    // long-standing "phantom replay" junk).
    for (uint32_t index = 0; index < received && index < DATA_EP_MAX_PACKET; ++index) {
        if (userPort) {
            userRingPushRx(buffer[index]);
        } else {
            markResetCauseIfUnset(USBD_DIAG_CAUSE_SERVICE_DATA_RX);
            ringPushRx(buffer[index]);
        }
    }
    // After EVENTS_ENDEPOUT, the nRF52840 USBD endpoint automatically re-enables itself
    // to accept the next OUT packet from the host (per PS section on Bulk OUT state machine).
    // Do NOT call queueDataOut here — that would fire a spurious TASKS_STARTEPOUT with no
    // data in the USB internal buffer, recreating the same DMA-pending hang that we just fixed
    // in startCdcEndpoints. The next TASKS_STARTEPOUT will be issued by the EPDATA handler
    // when the next packet actually arrives.
}

void NrfUsbdDriver::serviceDataIn(bool userPort) {
    volatile bool &dataInFlight = userPort ? userDataInFlight_ : dataInFlight_;
    if (!configured_ || dataInFlight || dmaDirection_ != DmaDirection::None ||
        (userPort ? userTxPending() : txPending()) == 0U) {
        return;
    }

    uint8_t *txBuffer = userPort ? &userTxBuffer_[0] : &this->txBuffer_[0];
    volatile size_t &txHead = userPort ? userTxHead_ : txHead_;
    volatile size_t &txTail = userPort ? userTxTail_ : txTail_;
    uint8_t *endpointBuffer = userPort ? &userEndpointInBuffer_[0] : &endpointInBuffer_[0];
    const uint8_t endpoint = userPort ? USER_DATA_EP : SERVICE_DATA_EP;
    size_t count = 0U;
    size_t nextTail = txTail;
    while (count < DATA_EP_MAX_PACKET && nextTail != txHead) {
        endpointBuffer[count++] = txBuffer[nextTail];
        nextTail = (nextTail + 1U) % USBD_RING_BUFFER_SIZE;
    }

    if (count == 0U) {
        return;
    }

    if (!userPort) {
        diagResetAtUsbdBeginStage(16UL);
    }
    reg32(USBD_BASE, epinPtrOffset(endpoint)) = reinterpret_cast<uint32_t>(endpointBuffer);
    reg32(USBD_BASE, epinMaxcntOffset(endpoint)) = static_cast<uint32_t>(count);
    if (!startEasyDma(DmaDirection::In, endpoint)) {
        return;
    }
    txTail = nextTail;
    dataInFlight = true;
    if (!userPort) {
        diagResetAtUsbdBeginStage(17UL);
    }
}

void NrfUsbdDriver::serviceNotificationIn(bool userPort) {
    volatile bool &notificationInFlight = userPort ? userNotificationInFlight_ : notificationInFlight_;
    volatile bool &notificationPending = userPort ? userNotificationPending_ : notificationPending_;
    if (!configured_ || notificationInFlight || !notificationPending ||
        dmaDirection_ != DmaDirection::None) {
        return;
    }

    uint8_t *buffer = userPort ? &userNotificationBuffer_[0] : &notificationBuffer_[0];
    const uint8_t endpoint = userPort ? USER_NOTIFICATION_EP : SERVICE_NOTIFICATION_EP;
    reg32(USBD_BASE, epinPtrOffset(endpoint)) = reinterpret_cast<uint32_t>(buffer);
    reg32(USBD_BASE, epinMaxcntOffset(endpoint)) = 10UL;
    if (!userPort) {
        diagResetAtUsbdBeginStage(19UL);
    }
    if (!startEasyDma(DmaDirection::In, endpoint)) {
        return;
    }
    notificationPending = false;
    notificationInFlight = true;
    if (!userPort) {
        diagResetAtUsbdBeginStage(20UL);
    }
}

void NrfUsbdDriver::serviceDynamicEndpoints() {
    for (uint8_t endpoint = firstDynamicEndpoint(); endpoint < USB_MAX_ENDPOINTS; ++endpoint) {
        if (dynamicInBusy_[endpoint] && reg32(USBD_BASE, eventEndEpinOffset(endpoint)) != 0UL) {
            (void)completeEasyDma(DmaDirection::In, endpoint);
            reg32(USBD_BASE, eventEndEpinOffset(endpoint)) = 0UL;
            dynamicInBusy_[endpoint] = false;
            dynamicInLengths_[endpoint] = 0U;
            PluggableUSB().endpointInComplete(endpoint);
        }
    }
}

void NrfUsbdDriver::serviceSetup() {
    resetEp0InXferState();

    const uint8_t requestType = static_cast<uint8_t>(reg32(USBD_BASE, BMREQUESTTYPE) & 0xFFUL);
    const uint8_t request = static_cast<uint8_t>(reg32(USBD_BASE, BREQUEST) & 0xFFUL);
    const uint16_t value = static_cast<uint16_t>((reg32(USBD_BASE, WVALUEH) << 8U) | reg32(USBD_BASE, WVALUEL));
    const uint16_t index = static_cast<uint16_t>((reg32(USBD_BASE, WINDEXH) << 8U) | reg32(USBD_BASE, WINDEXL));
    const uint16_t length = static_cast<uint16_t>((reg32(USBD_BASE, WLENGTHH) << 8U) | reg32(USBD_BASE, WLENGTHL));
    ep0InRequested_ = length;

    switch (requestType & 0x60U) {
        case USB_REQ_TYPE_STANDARD:
            handleStandardRequest(request, value, index, length);
            break;
        case USB_REQ_TYPE_CLASS:
            handleClassRequest(requestType, request, value, index, length);
            break;
        default:
            stallControlEndpoint();
            break;
    }
}

void NrfUsbdDriver::completeControlOutTransfer() {
    controlOutLength_ = reg32(USBD_BASE, epoutAmountOffset(0U));

    switch (pendingControlOut_) {
        case ControlOutTransfer::ServiceLineCoding:
            if (controlOutLength_ >= 7U) {
                lineCoding_.baudRate =
                    static_cast<uint32_t>(controlOutBuffer_[0]) |
                    (static_cast<uint32_t>(controlOutBuffer_[1]) << 8U) |
                    (static_cast<uint32_t>(controlOutBuffer_[2]) << 16U) |
                    (static_cast<uint32_t>(controlOutBuffer_[3]) << 24U);
                lineCoding_.stopBits = controlOutBuffer_[4];
                lineCoding_.parity = controlOutBuffer_[5];
                lineCoding_.dataBits = controlOutBuffer_[6];
                if (lineCoding_.baudRate != 1200UL) {
                    serviceSawNonResetBaud_ = true;
                    // Don't let the host's post-touch 115200 re-open cancel a
                    // touch that is already counting down its confirm window.
                    const bool inConfirmWindow = serviceTouchResetMillis_ != 0UL &&
                        (millis() - serviceTouchResetMillis_) < USBD_TOUCH_RESET_CONFIRM_MS;
                    if (!inConfirmWindow) {
                        serviceTouchPending_ = false;
                        serviceTouchResetMillis_ = 0UL;
                    }
                } else {
                    markResetCauseIfUnset(USBD_DIAG_CAUSE_1200_LINE_CODING);
                    serviceSaw1200Millis_ = millis();
                    const bool resetArmed = configuredMillis_ != 0UL &&
                        (millis() - configuredMillis_) >= USBD_1200_RESET_ARM_MS;
                    if (!dtr_) {  // touch always armed (any profile, incl. debug)
                        markResetCauseIfUnset(USBD_DIAG_CAUSE_1200_TOUCH_PENDING);
                        serviceTouchPending_ = true;
                        serviceTouchResetMillis_ = resetArmed ? millis() : 0UL;
                    }
                }
            }
            sendZeroLengthStatus();
            break;
        case ControlOutTransfer::UserLineCoding:
            if (controlOutLength_ >= 7U) {
                userLineCoding_.baudRate =
                    static_cast<uint32_t>(controlOutBuffer_[0]) |
                    (static_cast<uint32_t>(controlOutBuffer_[1]) << 8U) |
                    (static_cast<uint32_t>(controlOutBuffer_[2]) << 16U) |
                    (static_cast<uint32_t>(controlOutBuffer_[3]) << 24U);
                userLineCoding_.stopBits = controlOutBuffer_[4];
                userLineCoding_.parity = controlOutBuffer_[5];
                userLineCoding_.dataBits = controlOutBuffer_[6];
            }
            sendZeroLengthStatus();
            break;
        case ControlOutTransfer::None:
        default:
            break;
    }

    pendingControlOut_ = ControlOutTransfer::None;
    controlOutExpected_ = 0U;
    controlOutLength_ = 0U;
}

void NrfUsbdDriver::handleStandardRequest(uint8_t request, uint16_t value, uint16_t index, uint16_t length) {
    const uint8_t requestType = static_cast<uint8_t>(reg32(USBD_BASE, BMREQUESTTYPE) & 0xFFUL);
    const uint8_t recipient = static_cast<uint8_t>(reg32(USBD_BASE, BMREQUESTTYPE) & 0x1FU);

    switch (request) {
        case USB_REQ_GET_DESCRIPTOR: {
            const uint8_t descriptorType = static_cast<uint8_t>((value >> 8U) & 0xFFU);
            const uint8_t descriptorIndex = static_cast<uint8_t>(value & 0xFFU);
            if (descriptorType == USB_DESC_DEVICE) {
                size_t descriptorLength = length;
                if (descriptorLength > sizeof(deviceDescriptor_)) {
                    descriptorLength = sizeof(deviceDescriptor_);
                }
                startControlIn(deviceDescriptor_, descriptorLength);
                return;
            }
            if (descriptorType == USB_DESC_CONFIGURATION) {
                size_t descriptorLength = length;
                if (descriptorLength > configurationDescriptorLength_) {
                    descriptorLength = configurationDescriptorLength_;
                }
                startControlIn(configurationDescriptor_, descriptorLength);
                return;
            }
            if (descriptorType == USB_DESC_STRING) {
                if (descriptorIndex == 0U) {
                    controlInBuffer_[0] = 4U;
                    controlInBuffer_[1] = USB_DESC_STRING;
                    controlInBuffer_[2] = 0x09U;
                    controlInBuffer_[3] = 0x04U;
                    startControlIn(controlInBuffer_, 4U);
                    return;
                }

                const NrfSystemProfile &profile = nrfSystemProfile();
                const char *text = nullptr;
                // iSerialNumber (descriptor index 3) must reproduce the
                // bootloader's chip-UID-derived serial so Windows treats
                // user firmware as the same device-instance as the
                // bootloader and reuses the same COM port assignment.
                // Adafruit's UF2 bootloader formats this from the 64-bit
                // FICR->DEVICEID value byte-wise in little-endian memory
                // order, then reverses the resulting hex string.
                // Matching that exact algorithm matters more than which raw
                // register looks semantically nicer.
                static char serialBuffer[17] = {0};
                if (descriptorIndex == USB_STRING_MANUFACTURER) {
                    text = "NiusRobotLab";
                } else if (descriptorIndex == USB_STRING_PRODUCT) {
                    text = profile.usbProduct;
                } else if (descriptorIndex == USB_STRING_SERIAL) {
                    constexpr uint32_t FICR_BASE = 0x10000000UL;
                    constexpr uint32_t FICR_DEVICEID0 = 0x060UL;
                    constexpr uint32_t FICR_DEVICEID1 = 0x064UL;
                    const uint32_t deviceId0 = reg32(FICR_BASE, FICR_DEVICEID0);
                    const uint32_t deviceId1 = reg32(FICR_BASE, FICR_DEVICEID1);
                    static const char hex[] = "0123456789ABCDEF";
                    const uint8_t deviceIdBytes[8] = {
                        static_cast<uint8_t>(deviceId0 & 0xFFU),
                        static_cast<uint8_t>((deviceId0 >> 8U) & 0xFFU),
                        static_cast<uint8_t>((deviceId0 >> 16U) & 0xFFU),
                        static_cast<uint8_t>((deviceId0 >> 24U) & 0xFFU),
                        static_cast<uint8_t>(deviceId1 & 0xFFU),
                        static_cast<uint8_t>((deviceId1 >> 8U) & 0xFFU),
                        static_cast<uint8_t>((deviceId1 >> 16U) & 0xFFU),
                        static_cast<uint8_t>((deviceId1 >> 24U) & 0xFFU),
                    };
                    for (size_t byteIndex = 0; byteIndex < 8U; ++byteIndex) {
                        const uint8_t value = deviceIdBytes[byteIndex];
                        for (size_t nibbleIndex = 0; nibbleIndex < 2U; ++nibbleIndex) {
                            const uint8_t nibble = static_cast<uint8_t>((value >> (nibbleIndex * 4U)) & 0x0FU);
                            serialBuffer[15U - ((byteIndex * 2U) + nibbleIndex)] = hex[nibble];
                        }
                    }
                    serialBuffer[16] = '\0';
                    text = serialBuffer;
                } else if (descriptorIndex == USB_STRING_SERVICE_CONTROL) {
                    text = "Nius Service CDC";
                } else if (descriptorIndex == USB_STRING_SERVICE_DATA) {
                    text = "Nius Service Data";
                } else if (descriptorIndex == USB_STRING_USER_CONTROL) {
                    text = "Nius User CDC";
                } else if (descriptorIndex == USB_STRING_USER_DATA) {
                    text = "Nius User Data";
                } else if (descriptorIndex == USB_STRING_DFU) {
                    text = "Nius Bootloader Control";
                } else {
                    text = "0001";
                }
                const size_t stringLength = copyUsbStringDescriptor(text, controlInBuffer_, sizeof(controlInBuffer_));
                size_t descriptorLength = length;
                if (descriptorLength > stringLength) {
                    descriptorLength = stringLength;
                }
                startControlIn(controlInBuffer_, descriptorLength);
                return;
            }

            USBSetup setup = {static_cast<uint8_t>(reg32(USBD_BASE, BMREQUESTTYPE) & 0xFFUL), request, value, index, length};
            PluggableUSB().beginDescriptorBuild(controlInBuffer_, sizeof(controlInBuffer_));
            const int descriptorLength = PluggableUSB().getDescriptor(setup);
            (void)PluggableUSB().endDescriptorBuild();
            if (descriptorLength > 0) {
                size_t responseLength = length;
                if (responseLength > static_cast<size_t>(descriptorLength)) {
                    responseLength = static_cast<size_t>(descriptorLength);
                }
                startControlIn(controlInBuffer_, responseLength);
                return;
            }

            stallControlEndpoint();
            return;
        }
        case USB_REQ_SET_ADDRESS:
            // USBD applies SET_ADDRESS in hardware; USBADDR is a read-only
            // status register. Keep only the software mirror used by status().
            setAddress(static_cast<uint8_t>(value & 0x7FU));
            pendingAddressValid_ = false;
            diagResetAtUsbdBeginStage(9UL);
            sendZeroLengthStatus();
            return;
        case USB_REQ_SET_CONFIGURATION:
            configuration_ = static_cast<uint8_t>(value & 0xFFU);
            configured_ = configuration_ != 0U;
            if (configured_) {
                configuredMillis_ = millis();
                startCdcEndpoints();
            } else {
                configuredMillis_ = 0UL;
                cdcActive_ = false;
                dataInFlight_ = false;
                notificationInFlight_ = false;
                notificationPending_ = false;
                userDataInFlight_ = false;
                userNotificationInFlight_ = false;
                userNotificationPending_ = false;
            }
            updateSerialState(false);
            if (userPortEnabled()) {
                updateSerialState(true);
            }
            diagResetAtUsbdBeginStage(10UL);
            sendZeroLengthStatus();
            diagResetAtUsbdBeginStage(11UL);
            return;
        case USB_REQ_GET_CONFIGURATION:
            controlInBuffer_[0] = configuration_;
            startControlIn(controlInBuffer_, 1U);
            return;
        case USB_REQ_GET_STATUS:
            controlInBuffer_[0] = 0U;
            controlInBuffer_[1] = 0U;
            startControlIn(controlInBuffer_, 2U);
            return;
        case USB_REQ_CLEAR_FEATURE:
        case USB_REQ_SET_FEATURE:
            if (recipient == USB_REQ_RECIPIENT_DEVICE || recipient == USB_REQ_RECIPIENT_INTERFACE || recipient == USB_REQ_RECIPIENT_ENDPOINT) {
                sendZeroLengthStatus();
                return;
            }
            stallControlEndpoint();
            return;
        case USB_REQ_GET_INTERFACE:
            controlInBuffer_[0] = 0U;
            startControlIn(controlInBuffer_, 1U);
            return;
        case USB_REQ_SET_INTERFACE:
            sendZeroLengthStatus();
            return;
        default:
            {
                USBSetup setup = {requestType, request, value, index, length};
                if (PluggableUSB().setup(setup)) {
                    sendZeroLengthStatus();
                    return;
                }
            }
            stallControlEndpoint();
            return;
    }
}

void NrfUsbdDriver::handleClassRequest(uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, uint16_t length) {
    diagResetAtUsbdBeginStage(12UL);
    const uint8_t recipient = requestType & 0x1FU;
    const bool directionIn = (requestType & USB_DIR_IN) != 0U;
    if (recipient != USB_REQ_RECIPIENT_INTERFACE) {
        stallControlEndpoint();
        return;
    }

    const uint8_t interfaceIndex = static_cast<uint8_t>(index & 0xFFU);
    if (interfaceIndex == SERVICE_CONTROL_INTERFACE || (userPortEnabled() && interfaceIndex == USER_CONTROL_INTERFACE)) {
        const bool userCdc = interfaceIndex == USER_CONTROL_INTERFACE;
        NrfUsbLineCoding &lineCoding = userCdc ? userLineCoding_ : lineCoding_;
        volatile bool &dtr = userCdc ? userDtr_ : dtr_;
        volatile bool &rts = userCdc ? userRts_ : rts_;
        switch (request) {
            case CDC_REQ_SET_LINE_CODING:
                {
                    size_t expectedLength = length;
                    if (expectedLength > sizeof(controlOutBuffer_)) {
                        expectedLength = sizeof(controlOutBuffer_);
                    }
                    expectControlOut(userCdc ? ControlOutTransfer::UserLineCoding : ControlOutTransfer::ServiceLineCoding, expectedLength);
                }
                diagResetAtUsbdBeginStage(13UL);
                return;
            case CDC_REQ_GET_LINE_CODING:
                controlInBuffer_[0] = static_cast<uint8_t>(lineCoding.baudRate & 0xFFU);
                controlInBuffer_[1] = static_cast<uint8_t>((lineCoding.baudRate >> 8U) & 0xFFU);
                controlInBuffer_[2] = static_cast<uint8_t>((lineCoding.baudRate >> 16U) & 0xFFU);
                controlInBuffer_[3] = static_cast<uint8_t>((lineCoding.baudRate >> 24U) & 0xFFU);
                controlInBuffer_[4] = lineCoding.stopBits;
                controlInBuffer_[5] = lineCoding.parity;
                controlInBuffer_[6] = lineCoding.dataBits;
                {
                    size_t responseLength = length;
                    if (responseLength > 7U) {
                        responseLength = 7U;
                    }
                    startControlIn(controlInBuffer_, responseLength);
                }
                diagResetAtUsbdBeginStage(14UL);
                return;
            case CDC_REQ_SET_CONTROL_LINE_STATE:
                {
                    dtr = (value & 0x0001U) != 0U;
                    rts = (value & 0x0002U) != 0U;
                    if (!userCdc && dtr && lineCoding_.baudRate != 1200UL) {
                        serviceSawNonResetBaud_ = true;
                    }
                    updateSerialState(userCdc);
                    const bool resetArmed = configuredMillis_ != 0UL &&
                        (millis() - configuredMillis_) >= USBD_1200_RESET_ARM_MS;
                    // Accept the touch if the line is at 1200 now OR was at 1200
                    // very recently: a host's single-port (usbcdc=disabled)
                    // sequence can drop DTR a few ms after the baud has already
                    // reverted, so requiring exact coincidence could miss the
                    // DTR-drop that arms the touch.
                    const bool recent1200 = lineCoding_.baudRate == 1200UL ||
                        (serviceSaw1200Millis_ != 0UL &&
                         (millis() - serviceSaw1200Millis_) < 400UL);
                    if (!userCdc && recent1200) {  // touch always armed (any profile)
                        if (!dtr) {
                            // Host dropped DTR at 1200 baud: arm the touch and start the 40 ms
                            // confirm timer (see USBD_TOUCH_RESET_CONFIRM_MS). The poll/IRQ gate
                            // confirms by writing the GPREGRET magic and triggering SYSRESETREQ.
                            markResetCauseIfUnset(USBD_DIAG_CAUSE_1200_TOUCH_PENDING);
                            serviceTouchPending_ = true;
                            serviceTouchResetMillis_ = resetArmed ? millis() : 0UL;
                        } else if (serviceTouchResetMillis_ == 0UL) {
                            // V1 latch fix: once the confirm window has started
                            // (serviceTouchResetMillis_ != 0) we must NOT cancel the pending
                            // touch on a subsequent DTR=true. Windows usbser.sys re-asserts
                            // DTR on SerialPort.Close() and again on the next CreateFile()
                            // (adafruit-nrfutil opens the port immediately after the touch),
                            // so the host-side touch sequence inevitably ends with DTR back
                            // high within tens of ms. Cancelling here would defeat the touch.
                            serviceTouchPending_ = false;
                        }
                    } else if (!userCdc) {
                        serviceTouchPending_ = false;
                        serviceTouchResetMillis_ = 0UL;
                    }
                }
                diagResetAtUsbdBeginStage(15UL);
                sendZeroLengthStatus();
                return;
            default:
                stallControlEndpoint();
                return;
        }
    }

#if !defined(NRF_USB_RUNTIME_DISABLE_APP_DFU) || (NRF_USB_RUNTIME_DISABLE_APP_DFU == 0)
    if (interfaceIndex == dfuInterfaceNumber()) {
        switch (request) {
            case DFU_REQ_DETACH:
                detachCause_ = USBD_DIAG_CAUSE_DFU_DETACH;
                detachRequestMagic_ = USBD_DETACH_REQUEST_MAGIC;
                sendZeroLengthStatus();
                return;
            case DFU_REQ_GETSTATUS:
                controlInBuffer_[0] = DFU_STATUS_OK;
                controlInBuffer_[1] = 1U;
                controlInBuffer_[2] = 0U;
                controlInBuffer_[3] = 0U;
                controlInBuffer_[4] = DFU_STATE_APP_IDLE;
                controlInBuffer_[5] = 0U;
                {
                    size_t responseLength = length;
                    if (responseLength > 6U) {
                        responseLength = 6U;
                    }
                    startControlIn(controlInBuffer_, responseLength);
                }
                return;
            case DFU_REQ_GETSTATE:
                controlInBuffer_[0] = DFU_STATE_APP_IDLE;
                startControlIn(controlInBuffer_, 1U);
                return;
            default:
                stallControlEndpoint();
                return;
        }
    }
#endif

    {
        USBSetup setup = {requestType, request, value, index, length};
        if (directionIn) {
            PluggableUSB().beginDescriptorBuild(controlInBuffer_, sizeof(controlInBuffer_));
            const int responseLength = PluggableUSB().getSetupResponse(setup);
            (void)PluggableUSB().endDescriptorBuild();
            if (responseLength > 0) {
                size_t actualLength = length;
                if (actualLength > static_cast<size_t>(responseLength)) {
                    actualLength = static_cast<size_t>(responseLength);
                }
                startControlIn(controlInBuffer_, actualLength);
                return;
            }
        } else if (PluggableUSB().setup(setup)) {
            sendZeroLengthStatus();
            return;
        }
    }

    stallControlEndpoint();
}

void NrfUsbdDriver::resetEp0InXferState() {
    ep0InXferPhase_ = Ep0InXferPhase::Idle;
    ep0InRemaining_ = 0U;
    ep0InRequested_ = 0U;
    ep0InCursor_ = nullptr;
    ep0InNeedsZlp_ = false;
}

void NrfUsbdDriver::abortEp0Transfer() {
    if (dmaEndpoint_ == 0U && dmaDirection_ != DmaDirection::None) {
        // A new SETUP or USBRESET makes the previous EP0 transaction defunct;
        // no matching END is required after the MAC has reported that abort.
        // If END was already latched, however, its actual AMOUNT completed and
        // must contribute to the cumulative disable/re-enable parity.
        const DmaDirection direction = dmaDirection_;
        const uint32_t endEvent = direction == DmaDirection::In
            ? eventEndEpinOffset(0U)
            : eventEndEpoutOffset(0U);
        if (reg32(USBD_BASE, endEvent) != 0UL) {
            (void)completeEasyDma(direction, 0U);
        } else {
            usbErrata199EndDma();
            dmaDirection_ = DmaDirection::None;
            dmaEndpoint_ = 0xFFU;
        }
    }
    resetEp0InXferState();
    pendingControlOut_ = ControlOutTransfer::None;
    controlOutExpected_ = 0U;
    controlOutLength_ = 0U;
    pendingAddressValid_ = false;
    pendingAddress_ = 0U;
    reg32(USBD_BASE, eventEndEpinOffset(0U)) = 0UL;
    reg32(USBD_BASE, eventEndEpoutOffset(0U)) = 0UL;
    reg32(USBD_BASE, EVENTS_EP0DATADONE) = 0UL;
}

void NrfUsbdDriver::sendEp0ControlInChunkOrAdvanceStatus() {
    if (ep0InXferPhase_ != Ep0InXferPhase::Data) {
        return;
    }
    if (ep0InRemaining_ > 0U) {
        size_t chunk = ep0InRemaining_;
        if (chunk > CONTROL_EP_MAX_PACKET) {
            chunk = CONTROL_EP_MAX_PACKET;
        }
        std::memmove(controlInBuffer_, ep0InCursor_, chunk);
        ep0InCursor_ += chunk;
        ep0InRemaining_ = static_cast<uint16_t>(ep0InRemaining_ - static_cast<uint16_t>(chunk));
        reg32(USBD_BASE, epinPtrOffset(0U)) = reinterpret_cast<uint32_t>(&controlInBuffer_[0]);
        reg32(USBD_BASE, epinMaxcntOffset(0U)) = static_cast<uint32_t>(chunk);
        (void)startEasyDma(DmaDirection::In, 0U);
        return;
    }
    if (ep0InNeedsZlp_) {
        ep0InNeedsZlp_ = false;
        reg32(USBD_BASE, epinPtrOffset(0U)) = reinterpret_cast<uint32_t>(&controlInBuffer_[0]);
        reg32(USBD_BASE, epinMaxcntOffset(0U)) = 0UL;
        (void)startEasyDma(DmaDirection::In, 0U);
        return;
    }
    ep0InXferPhase_ = Ep0InXferPhase::StatusPending;
    sendZeroLengthStatus();
}

void NrfUsbdDriver::startControlIn(const uint8_t *data, size_t length) {
    ep0InXferPhase_ = Ep0InXferPhase::Data;
    const uint16_t actualLength = static_cast<uint16_t>(length > 0xFFFFU ? 0xFFFFU : length);
    ep0InRemaining_ = actualLength;
    ep0InCursor_ = data;
    ep0InNeedsZlp_ = controlInNeedsZlp(actualLength, ep0InRequested_);
    sendEp0ControlInChunkOrAdvanceStatus();
}

void NrfUsbdDriver::expectControlOut(ControlOutTransfer transferType, size_t length) {
    pendingControlOut_ = transferType;
    controlOutExpected_ = length;
    controlOutLength_ = 0U;
    reg32(USBD_BASE, epoutPtrOffset(0U)) = reinterpret_cast<uint32_t>(&controlOutBuffer_[0]);
    reg32(USBD_BASE, epoutMaxcntOffset(0U)) = static_cast<uint32_t>(length);
    reg32(USBD_BASE, TASKS_EP0RCVOUT) = 1UL;
}

void NrfUsbdDriver::sendZeroLengthStatus() {
    reg32(USBD_BASE, epinPtrOffset(0U)) = reinterpret_cast<uint32_t>(&controlInBuffer_[0]);
    reg32(USBD_BASE, epinMaxcntOffset(0U)) = 0UL;
    reg32(USBD_BASE, TASKS_EP0STATUS) = 1UL;
}

void NrfUsbdDriver::stallControlEndpoint() {
    reg32(USBD_BASE, TASKS_EP0STALL) = 1UL;
}

void NrfUsbdDriver::completePendingAddress() {
    if (!pendingAddressValid_) {
        return;
    }
    setAddress(pendingAddress_);
    pendingAddressValid_ = false;
}

void NrfUsbdDriver::queueSerialStateNotification(bool userPort) {
    uint8_t *buffer = userPort ? &userNotificationBuffer_[0] : &notificationBuffer_[0];
    volatile uint16_t &serialStateBitmap = userPort ? userSerialStateBitmap_ : serialStateBitmap_;
    volatile bool &notificationPending = userPort ? userNotificationPending_ : notificationPending_;
    buffer[0] = 0xA1U;
    buffer[1] = CDC_NOTIFICATION_SERIAL_STATE;
    buffer[2] = 0x00U;
    buffer[3] = 0x00U;
    buffer[4] = userPort ? USER_CONTROL_INTERFACE : SERVICE_CONTROL_INTERFACE;
    buffer[5] = 0x00U;
    buffer[6] = 0x02U;
    buffer[7] = 0x00U;
    buffer[8] = static_cast<uint8_t>(serialStateBitmap & 0xFFU);
    buffer[9] = static_cast<uint8_t>((serialStateBitmap >> 8U) & 0xFFU);
    notificationPending = true;
}

void NrfUsbdDriver::updateSerialState(bool userPort) {
    const bool asserted = userPort ? (userDtr_ || userRts_) : (dtr_ || rts_);
    volatile uint16_t &serialStateBitmap = userPort ? userSerialStateBitmap_ : serialStateBitmap_;
    uint16_t nextState = 0U;
    if (configured_ && asserted) {
        nextState = CDC_SERIAL_STATE_DCD | CDC_SERIAL_STATE_DSR;
    }
    if (nextState == serialStateBitmap) {
        return;
    }
    serialStateBitmap = nextState;
    queueSerialStateNotification(userPort);
}

void NrfUsbdDriver::setAddress(uint8_t address) {
    address_ = address;
}

void NrfUsbdDriver::ringPushRx(uint8_t value) {
    ringPush(rxBuffer_, rxHead_, rxTail_, value);
}

bool NrfUsbdDriver::ringPushTx(uint8_t value) {
    return ringPushWithResult(txBuffer_, txHead_, txTail_, value);
}

int NrfUsbdDriver::ringPopRx() {
    return ringPop(rxBuffer_, rxHead_, rxTail_);
}

int NrfUsbdDriver::ringPeekRx() const {
    return ringPeek(rxBuffer_, rxHead_, rxTail_);
}

size_t NrfUsbdDriver::txPending() const {
    return ringPending(txHead_, txTail_);
}

void NrfUsbdDriver::userRingPushRx(uint8_t value) {
    ringPush(userRxBuffer_, userRxHead_, userRxTail_, value);
}

bool NrfUsbdDriver::userRingPushTx(uint8_t value) {
    return ringPushWithResult(userTxBuffer_, userTxHead_, userTxTail_, value);
}

int NrfUsbdDriver::userRingPopRx() {
    return ringPop(userRxBuffer_, userRxHead_, userRxTail_);
}

int NrfUsbdDriver::userRingPeekRx() const {
    return ringPeek(userRxBuffer_, userRxHead_, userRxTail_);
}

size_t NrfUsbdDriver::userTxPending() const {
    return ringPending(userTxHead_, userTxTail_);
}

extern "C" void USBD_IRQHandler(void) {
    nrfUsbdDriver().irqHandler();
}
