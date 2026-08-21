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
constexpr uint16_t USB_FEATURE_ENDPOINT_HALT = 0x0000U;
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
// nRF52840 anomaly 199 workaround used by Nordic's nrfx USBD driver. While
// EasyDMA owns the peripheral, this latch prevents USB tasks/tokens from being
// lost. TaichiUSB serializes every endpoint DMA and brackets it with the same
// latch so EP0, both CDC functions, and pluggable endpoints cannot collide.
constexpr uint32_t USBD_DMA_TASK_HOLD = 0x40027C1CUL;
constexpr uint32_t USBD_DMA_TASK_HOLD_ENABLE = 0x00000082UL;
constexpr uint32_t POWER_BASE = 0x40000000UL;
constexpr uint32_t CLOCK_BASE = 0x40000000UL;
constexpr uint32_t TASKS_STARTEPIN_BASE = 0x004UL;
constexpr uint32_t TASKS_STARTEPOUT_BASE = 0x028UL;
constexpr uint32_t TASKS_EP0RCVOUT = 0x04CUL;
constexpr uint32_t TASKS_EP0STATUS = 0x050UL;
constexpr uint32_t TASKS_EP0STALL = 0x054UL;
constexpr uint32_t EVENTS_USBRESET = 0x100UL;
constexpr uint32_t EVENTS_STARTED = 0x104UL;
constexpr uint32_t EVENTS_ENDEPIN_BASE = 0x108UL;
constexpr uint32_t EVENTS_ENDEPOUT_BASE = 0x130UL;
constexpr uint32_t EVENTS_EP0DATADONE = 0x128UL;
constexpr uint32_t EVENTS_USBEVENT = 0x158UL;
constexpr uint32_t EVENTS_EP0SETUP = 0x15CUL;
constexpr uint32_t EVENTS_EPDATA = 0x160UL;
// nRF52840 USBD register map: EPSTATUS=0x468 (which endpoints' EasyDMA was
// captured), EPDATASTATUS=0x46C (which endpoints had an acknowledged host data
// transfer — the "this OUT packet is buffered, drain it" signal). USBADDR at
// 0x470 is read-only and must never be treated as an initialization register.
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
constexpr uint32_t DTOGGLE = 0x50CUL;
constexpr uint32_t EPINEN = 0x510UL;
constexpr uint32_t EPOUTEN = 0x514UL;
constexpr uint32_t EPSTALL = 0x518UL;
constexpr uint32_t USBD_EPSTALL_STALL = 1UL << 8U;
constexpr uint32_t EPOUT_PTR_BASE = 0x700UL;
constexpr uint32_t EPOUT_MAXCNT_BASE = 0x704UL;
constexpr uint32_t EPOUT_AMOUNT_BASE = 0x708UL;
constexpr uint32_t EPIN_PTR_BASE = 0x600UL;
constexpr uint32_t EPIN_MAXCNT_BASE = 0x604UL;
constexpr uint32_t USBD_ENDPOINT_CLUSTER_STRIDE = 0x14UL;
constexpr uint32_t POWER_USBREGSTATUS = 0x438UL;
constexpr uint32_t POWER_USBREGSTATUS_VBUSDETECT = 1UL;
constexpr uint32_t POWER_GPREGRET = 0x51CUL;
constexpr uint32_t NVIC_ISER_BASE = 0xE000E100UL;
constexpr uint32_t NVIC_ICER_BASE = 0xE000E180UL;
constexpr uint32_t CLOCK_TASKS_HFCLKSTART = 0x000UL;
constexpr uint32_t CLOCK_EVENTS_HFCLKSTARTED = 0x100UL;
constexpr uint32_t AIRCR = 0xE000ED0CUL;
constexpr uint32_t AIRCR_RESET_KEY = 0x05FA0000UL;
constexpr uint32_t AIRCR_SYSRESETREQ = 0x4UL;
extern "C" volatile uint32_t g_nrfDiagCause;
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
// service-CDC OUT data-ready bit never matched, its DMA was never scheduled,
// and the first host OUT packet sat undrained in the endpoint's
// internal buffer — wedging every subsequent OUT into a NAK. That was the
// real cause of the GDB-stub "service CDC OUT NAKs while halted" bug: the
// stub is simply the first feature to receive bulk OUT on the service CDC
// (uploads only use EP0 control), so the latent off-by-one finally bit.
constexpr uint32_t USBD_INT_EPDATA_MASK = (1UL << 24);
constexpr uint32_t EPDATASTATUS_OUT_BASE_BIT = 17U;
constexpr uint32_t USBD_EVENTCAUSE_SUSPEND_MASK = (1UL << 8);
constexpr uint32_t USBD_EVENTCAUSE_RESUME_MASK = (1UL << 9);
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
// Bounded spin for flush() while the GDB stub is halted (ISR-mode). Generous —
// the USBD ISR drains the IN ring between kicks — but finite so a host that
// stops reading can't wedge the stub forever.
constexpr uint32_t USBD_STUB_FLUSH_SPINS = 2000000UL;
constexpr size_t USBD_RING_BUFFER_SIZE = 256U;
#if defined(NRF_USBD_1200_RESET_ARM_MS)
constexpr uint32_t USBD_1200_RESET_ARM_MS = static_cast<uint32_t>(NRF_USBD_1200_RESET_ARM_MS);
#else
// The trigger already requires the maintenance CDC to be configured, receive
// an explicit 1200-bps line-coding request, and observe a DTR falling edge.
// Three seconds added no meaningful protection and made a second upload issued
// immediately after re-enumeration silently lose its touch. Windows can replay
// the port's previous line coding during the first few hundred milliseconds of
// enumeration, however, so the guard must extend past that setup traffic. The
// 400 ms boundary is the shortest value that survived repeated physical reset
// and CDC-traffic cycles on the ProMicro nRF52840 route.
constexpr uint32_t USBD_1200_RESET_ARM_MS = 400UL;
#endif
#if defined(NRF_USBD_IGNORE_INITIAL_1200_RESET_COUNT)
constexpr uint8_t USBD_IGNORE_INITIAL_1200_RESET_COUNT = static_cast<uint8_t>(NRF_USBD_IGNORE_INITIAL_1200_RESET_COUNT);
#else
constexpr uint8_t USBD_IGNORE_INITIAL_1200_RESET_COUNT = 0U;
#endif
constexpr uint32_t USBD_TOUCH_RESET_CONFIRM_MS = 40UL;
// usbser may assert DTR before its first bulk-IN read is posted. A short
// post-DTR guard prevents the application's first bytes from being committed
// to the endpoint FIFO before Windows is ready to consume them.
constexpr uint32_t USBD_CDC_OPEN_SETTLE_MS = 10UL;

static_assert(USBD_INT_ENDEPOUT2_MASK == (1UL << 14),
              "nRF52840 ENDEPOUT2 interrupt position drifted");
static_assert(USBD_INT_ENDEPOUT4_MASK == (1UL << 16),
              "nRF52840 ENDEPOUT4 interrupt position drifted");

constexpr bool controlInNeedsZlp(size_t actualLength,
                                 size_t requestedLength,
                                 size_t maxPacket = CONTROL_EP_MAX_PACKET) {
    return actualLength > 0U && actualLength < requestedLength &&
           maxPacket > 0U && (actualLength % maxPacket) == 0U;
}

static_assert(controlInNeedsZlp(64U, 65U));
static_assert(!controlInNeedsZlp(64U, 64U));
static_assert(!controlInNeedsZlp(63U, 64U));

constexpr bool explicitTouchGesture(bool previousDtr,
                                    bool nextDtr,
                                    bool recent1200,
                                    bool resetArmed) {
    return previousDtr && !nextDtr && recent1200 && resetArmed;
}

static_assert(explicitTouchGesture(true, false, true, true));
static_assert(!explicitTouchGesture(false, false, true, true));
static_assert(!explicitTouchGesture(true, false, false, true));
static_assert(!explicitTouchGesture(true, false, true, false));

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

inline bool nvicIrqEnabled(uint32_t irqNumber) {
    return (reg32(NVIC_ISER_BASE, (irqNumber / 32UL) * 4UL) & (1UL << (irqNumber % 32UL))) != 0UL;
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
// Once the USBD interrupt is enabled, processBusState() /
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
// IRQ is temporarily disabled while the GDB stub holds it masked and
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
// host OUT. Skipping this write is what made this clone re-present stale
// FIFO content as phantom packets after every real one.
inline uint32_t sizeEpoutOffset(uint8_t endpoint) {
    return 0x4A0UL + (static_cast<uint32_t>(endpoint) * 4UL);
}

inline uint32_t epinPtrOffset(uint8_t endpoint) {
    return EPIN_PTR_BASE + (static_cast<uint32_t>(endpoint) * USBD_ENDPOINT_CLUSTER_STRIDE);
}

inline uint32_t epinMaxcntOffset(uint8_t endpoint) {
    return EPIN_MAXCNT_BASE + (static_cast<uint32_t>(endpoint) * USBD_ENDPOINT_CLUSTER_STRIDE);
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

inline void stallEndpoint(uint8_t endpointAddress) {
    reg32(USBD_BASE, EPSTALL) = USBD_EPSTALL_STALL | endpointAddress;
}

inline void resetEndpointDataState(uint8_t endpointAddress) {
    clearEndpointToggle(endpointAddress);
    clearEndpointStall(endpointAddress);
}

inline bool triggerEndpointStartTask(uint32_t taskOffset) {
    uint32_t endEventOffset = 0UL;
    if (taskOffset >= TASKS_STARTEPIN_BASE &&
        taskOffset <= taskStartEpinOffset(USB_MAX_ENDPOINTS - 1U)) {
        const uint8_t endpoint = static_cast<uint8_t>((taskOffset - TASKS_STARTEPIN_BASE) / 4UL);
        endEventOffset = eventEndEpinOffset(endpoint);
    } else if (taskOffset >= TASKS_STARTEPOUT_BASE &&
               taskOffset <= taskStartEpoutOffset(USB_MAX_ENDPOINTS - 1U)) {
        const uint8_t endpoint = static_cast<uint8_t>((taskOffset - TASKS_STARTEPOUT_BASE) / 4UL);
        endEventOffset = eventEndEpoutOffset(endpoint);
    } else {
        return false;
    }

    // EVENTS_STARTED means only that the task was captured, not that EasyDMA
    // is free for another endpoint. Waiting for END mirrors nrfx's stable
    // scheduler and keeps register accesses outside an active DMA window.
    reg32(USBD_BASE, endEventOffset) = 0UL;
    reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
    reg32(0UL, USBD_DMA_TASK_HOLD) = USBD_DMA_TASK_HOLD_ENABLE;
    reg32(USBD_BASE, taskOffset) = 1UL;
    asm volatile("dsb 0xf" ::: "memory");
    asm volatile("isb 0xf" ::: "memory");
    for (uint32_t spin = 0UL; spin < USBD_START_CAPTURE_TIMEOUT_SPINS; ++spin) {
        if (reg32(USBD_BASE, endEventOffset) != 0UL) {
            reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
            reg32(0UL, USBD_DMA_TASK_HOLD) = 0UL;
            return true;
        }
    }
    reg32(0UL, USBD_DMA_TASK_HOLD) = 0UL;
    return false;
}

inline bool vbusDetected() {
    return (reg32(POWER_BASE, POWER_USBREGSTATUS) & POWER_USBREGSTATUS_VBUSDETECT) != 0UL;
}

inline bool usbVbusAssumedPresent() {
#if defined(NRF_SYSTEM_USB_ASSUME_VBUS) && (NRF_SYSTEM_USB_ASSUME_VBUS == 1)
    return true;
#else
    return false;
#endif
}

inline bool effectiveVbusDetected() {
    return vbusDetected() || usbVbusAssumedPresent();
}

// nRF52840 POWER->USBREGSTATUS bit 1 = OUTPUTRDY (USB regulator stable).
// VBUSDETECT must be asserted before USBD->ENABLE. ENABLE then starts the
// dedicated regulator; OUTPUTRDY and controller READY must both be observed
// before the pull-up can be connected. A bootloader hand-off may inherit any
// intermediate state, so neither acknowledgement is inferred from it.
constexpr uint32_t POWER_USBREGSTATUS_OUTPUTRDY = (1UL << 1);

inline bool usbPwrRdy() {
    return (reg32(POWER_BASE, POWER_USBREGSTATUS) & POWER_USBREGSTATUS_OUTPUTRDY) != 0UL;
}

// nRF52840 Errata 187 ("USBD: USB cannot be enabled"): wraps the USBD
// ENABLE handshake with a trim-register sequence at 0x4006EC00 / 0x4006ED14.
// CRITICAL: the canonical Nordic nrfx_usbd implementation READS 0x4006EC00
// first; if it's already non-zero (e.g., the bootloader's nrfx_usbd primed
// it before handing off via NVIC_SystemReset), only the flag at 0x4006ED14
// is updated. Writing 0x9375 unconditionally to 0x4006EC00 CORRUPTS the
// trim and the USBD peripheral then enumerates with garbage descriptors
// (host sees the device but never binds it). Reproduced exactly as
// nrfx_usbd::usbd_errata_187_211_begin / _end.
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
// Same gating: read 0x4006EC00; if zero, do the full magic-flag-magic
// sequence; otherwise just update the flag at 0x4006EC14 (note: EC14, NOT
// the 0x4006ED14 used by 187). Magic value is 0xC0 (vs. 0x3 for 187).
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

inline bool ensureHfclk() {
    reg32(CLOCK_BASE, CLOCK_EVENTS_HFCLKSTARTED) = 0UL;
    reg32(CLOCK_BASE, CLOCK_TASKS_HFCLKSTART) = 1UL;
    for (uint32_t spin = 0; spin < 200000UL; ++spin) {
        if (reg32(CLOCK_BASE, CLOCK_EVENTS_HFCLKSTARTED) != 0UL) {
            return true;
        }
    }
    return false;
}


constexpr uint32_t USBD_BOOTLOADER_RESET_DISCONNECT_SPINS = 640000UL;

inline void requestBootloaderReset() {
    reg32(USBD_BASE, USBPULLUP) = 0UL;
    reg32(USBD_BASE, ENABLE) = 0UL;
    for (volatile uint32_t spin = 0UL; spin < USBD_BOOTLOADER_RESET_DISCONNECT_SPINS; ++spin) {
        __asm volatile("nop");
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
    g_nrfDiagCause = cause;
}

inline void markResetCauseIfUnset(uint32_t cause) {
    if (g_nrfDiagCause == 0UL) {
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

void NrfUsbdDriver::begin() {
    if (!nrfUsbRuntimeEnabled()) {
        return;
    }
    startRequested_ = true;
    if (enabled_ || startupInProgress_ || startupFaulted_) {
        return;
    }
    // The nRF52840 product specification requires VBUS before ENABLE. Prove the
    // selected board's VBUS contract (physical detect, or an explicit board
    // profile assertion). Keep the request pending when that proof is absent;
    // yield()-driven poll() retries after cable insertion instead of publishing
    // a controller that never became READY.
    if (!effectiveVbusDetected()) {
        return;
    }
    startupInProgress_ = true;

    // A SoftDevice/UF2 bootloader may jump directly to the application while
    // leaving USBD and its pullup enabled. Waiting before detaching keeps the
    // bootloader identity electrically present, so Windows can carry the old
    // control/bulk session into the application and expose a COM port whose
    // transfers never complete. Detach first, then honor the physically tested
    // 400 ms host handoff guard. A hardware-clean reset still needs this quiet
    // interval because Windows may be retiring the preceding bootloader node.
    reg32(USBD_BASE, USBPULLUP) = 0UL;
    reg32(USBD_BASE, ENABLE) = 0UL;
    // ENABLE readback remains Enabled until hardware has actually released the
    // inherited session. The disconnect dwell starts only after that ownership
    // boundary; otherwise a delayed disable can race the new enable.
    if (!spinUntil([]() { return reg32(USBD_BASE, ENABLE) == 0UL; })) {
        startupFaulted_ = true;
        startupInProgress_ = false;
        return;
    }
    delay(400);
    if (!effectiveVbusDetected()) {
        startupInProgress_ = false;
        return;
    }

    diagResetAtUsbdBeginStage(1UL);
    if (!ensureHfclk()) {
        startupFaulted_ = true;
        startupInProgress_ = false;
        return;
    }
    if (!initDescriptors()) {
        // Descriptor construction happens while USBD is still disabled. Keep
        // the pull-up disconnected rather than exposing a truncated composite
        // configuration that can poison the host's descriptor cache.
        startupFaulted_ = true;
        startupInProgress_ = false;
        return;
    }
    diagResetAtUsbdBeginStage(2UL);

    // Enabling USBD starts the dedicated USB regulator. OUTPUTRDY must
    // therefore be proved after ENABLE, along with controller READY, and
    // before connecting the D+ pull-up.
    diagResetAtUsbdBeginStage(3UL);

    // Errata 187+171 wrap the ENABLE handshake (Nordic's nrfx_usbd applies
    // both, in this nesting order: 187 outer, 171 inner). Both target the
    // same trim block but at different flag offsets — 187 at 0x4006ED14
    // with 0x3, 171 at 0x4006EC14 with 0xC0. The conditional reads of
    // 0x4006EC00 keep them safe to call after the bootloader's nrfx_usbd
    // has already primed the trim register.
    usbErrata187First();
    usbErrata171First();

    // Abort an unacknowledged enable without ever publishing software READY.
    // The hidden-register errata brackets may be closed only after ENABLE
    // reads back Disabled. If readback itself fails, retain their ownership and
    // make the driver terminal until chip reset; guessing at another enable
    // would compound an already-unknown peripheral state.
    auto abortEnable = [this](bool errataOpen, bool terminal) {
        reg32(USBD_BASE, USBPULLUP) = 0UL;
        reg32(USBD_BASE, ENABLE) = 0UL;
        const bool disabled = spinUntil([]() {
            return reg32(USBD_BASE, ENABLE) == 0UL;
        });
        if (disabled && errataOpen) {
            usbErrata171Second();
            usbErrata187Second();
        }
        if (disabled) {
            clearEvents();
        }
        disableInterrupts();
        enabled_ = false;
        started_ = false;
        ready_ = false;
        configured_ = false;
        suspended_ = false;
        cdcActive_ = false;
        startupFaulted_ = terminal;
        startupInProgress_ = false;
        resetConnectionState();
    };

    clearEvents();
    reg32(USBD_BASE, ENABLE) = USBD_ENABLE_VALUE;
    diagResetAtUsbdBeginStage(4UL);

    // Wait for the USBEVENT::READY event (EVENTCAUSE bit 11). This flag
    // signals that the USBD peripheral's internal state machine has fully
    // initialized after ENABLE=1 and that endpoint configuration writes
    // are now safe.
    auto waitUsbReadyEvent = []() -> bool {
        return spinUntil([]() {
            return (reg32(USBD_BASE, EVENTCAUSE) & USBD_EVENTCAUSE_READY_MASK) != 0UL;
        });
    };

    bool readyObserved = waitUsbReadyEvent();
    if (!readyObserved) {
        // Serial DFU -> app transitions occasionally leave USBD between states;
        // make one bounded, fully bracketed retry. Confirm Disabled before
        // closing the first Errata 171/187 transaction, then open a fresh
        // transaction for the second ENABLE. Do not repeatedly reset the
        // controller or advertise a USB identity when either handshake fails.
        reg32(USBD_BASE, USBPULLUP) = 0UL;
        reg32(USBD_BASE, ENABLE) = 0UL;
        if (!spinUntil([]() { return reg32(USBD_BASE, ENABLE) == 0UL; })) {
            abortEnable(true, true);
            return;
        }
        usbErrata171Second();
        usbErrata187Second();
        if (!effectiveVbusDetected()) {
            abortEnable(false, false);
            return;
        }
        for (volatile uint32_t n = 0UL; n < 640000UL; ++n) {
            (void)n;
        }
        usbErrata187First();
        usbErrata171First();
        clearEvents();
        reg32(USBD_BASE, ENABLE) = USBD_ENABLE_VALUE;
        readyObserved = waitUsbReadyEvent();
    }
    if (!readyObserved) {
        abortEnable(true, effectiveVbusDetected());
        return;
    }
    reg32(USBD_BASE, EVENTCAUSE) = USBD_EVENTCAUSE_READY_MASK;
    diagResetAtUsbdBeginStage(5UL);

    // Reverse-order wrap exit: 171 inner end, then 187 outer end.
    usbErrata171Second();
    usbErrata187Second();

    // Re-check OUTPUTRDY after ENABLE. Bootloader hand-off can leave the
    // regulator transition in flight; this keeps the first pullup / EP
    // activity from racing the analog block.
    if (!spinUntil([]() { return usbPwrRdy(); })) {
        abortEnable(false, effectiveVbusDetected());
        return;
    }

    enabled_ = true;
    started_ = true;
    attached_ = true;
    resetConnectionState();
    // The USBD peripheral completed its post-ENABLE READY handshake above, so it
    // is ready for endpoint activity regardless of bus state. Set it here (and
    // only clear it in end()), because the one-shot EVENTCAUSE.READY event was
    // already consumed during the ENABLE handshake and won't fire again.
    ready_ = true;
    // Enable the USBD interrupt only now that all device state is initialized,
    // and immediately before engaging the pullup that makes the host begin
    // enumeration. From here on, enumeration is serviced in the ISR and is
    // therefore immune to whatever the user sketch's loop() does.
    enableInterrupts();
    enablePullup(attached_ && effectiveVbusDetected());
    configStartMillis_ = millis();
    startupInProgress_ = false;

    diagResetAtUsbdBeginStage(6UL);
}

void NrfUsbdDriver::end() {
    enablePullup(false);
    disableInterrupts();
    reg32(USBD_BASE, ENABLE) = 0UL;
    enabled_ = false;
    started_ = false;
    attached_ = false;
    startRequested_ = false;
    startupInProgress_ = false;
    ready_ = false;
    resetConnectionState();
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
    reg32(USBD_BASE, EPINEN) = 0UL;
    reg32(USBD_BASE, EPOUTEN) = 0UL;
}

void NrfUsbdDriver::attach() {
    if (!nrfUsbRuntimeEnabled()) {
        return;
    }
    startRequested_ = true;
    if (!enabled_) {
        begin();
        return;
    }

    attached_ = true;
    resetConnectionState();
    clearEvents();
    started_ = true;
    enablePullup(effectiveVbusDetected());
}

void NrfUsbdDriver::detach() {
    startRequested_ = false;
    if (!enabled_) {
        attached_ = false;
        return;
    }

    attached_ = false;
    enablePullup(false);
    resetConnectionState();
    clearEvents();
}

void NrfUsbdDriver::poll() {
    if (!enabled_) {
        if (startRequested_) {
            begin();
        }
        return;
    }

    if (pollTraceEnabled()) {
        ++g_usbdPollTrace.pollCalls;
    }

    // Serialize against the USBD ISR: poll() and irqHandler() run the same
    // servicing routines and touch the same registers/state. With the IRQ live
    // (interrupt-driven builds) this mask makes the whole foreground pass atomic
    // w.r.t. the ISR; while the GDB stub deliberately masks the IRQ this is a
    // no-op and the stub pumps the same service path explicitly.
    UsbdIrqLock lock;

    const bool hasVbus = effectiveVbusDetected();
    enablePullup(attached_ && hasVbus);
    processBusState(hasVbus);

    if (attached_ && hasVbus) {
        started_ = true;
    }

    // nRF52840 has one shared USBD EasyDMA engine. Do not launch a CDC,
    // notification, or pluggable-endpoint transfer while EP0 owns that engine;
    // doing so can replace the first-open GET/SET_LINE_CODING response and make
    // Windows wait for usbser.sys' 30-second control-transfer timeout.
    const bool controlPlaneIdle =
        ep0InXferPhase_ == Ep0InXferPhase::Idle &&
        pendingControlOut_ == ControlOutTransfer::None;
    if (attached_ && configured_ && !suspended_ && controlPlaneIdle) {
        serviceDataIn(false);
        serviceNotificationIn(false);
        if (userPortEnabled()) {
            serviceDataIn(true);
            serviceNotificationIn(true);
        }
    }

    serviceTouchTimer();

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
    if (!enabled_) {
        return;
    }

    if (pollTraceEnabled()) {
        ++g_usbdPollTrace.irqCalls;
    }

    processBusState(effectiveVbusDetected());
    const bool controlPlaneIdle =
        ep0InXferPhase_ == Ep0InXferPhase::Idle &&
        pendingControlOut_ == ControlOutTransfer::None;
    if (attached_ && configured_ && !suspended_ && controlPlaneIdle) {
        serviceDataIn(false);
        serviceNotificationIn(false);
        if (userPortEnabled()) {
            serviceDataIn(true);
            serviceNotificationIn(true);
        }
    }
    serviceTouchTimer();
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
// CDC has delivered the explicit 1200-bps/DTR-falling upload gesture before we
// reboot to the bootloader. The stub busy-loops the pump, so this is a debounce
// against a torn control transaction, not
// a wall-clock interval (millis() is frozen during the DebugMon halt).
// The halted stub has already observed a configured service CDC at 1200 baud;
// this is an explicit upload request, not ambient serial traffic. Keep a short
// poll debounce to reject a torn control transfer, but do not make recovery
// depend on thousands of USB service passes (some passes wait on EasyDMA and
// turned a valid touch into a tens-of-seconds escape).
constexpr uint32_t USBD_HALT_TOUCH_CONFIRM_TICKS = 32UL;

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
    // The maintenance CDC must receive both 1200-bps line coding and a DTR
    // falling edge. Merely selecting 1200 baud is not an upload request.
    // lineCoding_ is
    // updated from EP0 control-OUT (completeControlOutTransfer), which still
    // completes while halted — that is how re-enumeration finishes — so this
    // signal is observable even though millis() is frozen.
    // Honor the touch even while halted in the GDB stub debugger, on any profile -
    // a debug build must still be DFU-recoverable over USB.
    const bool touchSignal = enabled_ && configured_ && serviceTouchPending_ &&
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

void NrfUsbdDriver::processBusState(bool hasVbus) {
    if (!hasVbus) {
        if (enabled_) {
            // VBUS removal powers down the USB regulator. Keeping ENABLE and
            // the old configured/endpoint state across that boundary lets a
            // later cable insertion expose a stale session before OUTPUTRDY.
            // Preserve logical attach/start intent, but retire this controller
            // instance. The normal begin() path will prove disable readback,
            // open fresh errata brackets, wait for READY + OUTPUTRDY, and only
            // then reconnect the pull-up.
            enablePullup(false);
            disableInterrupts();
            reg32(USBD_BASE, ENABLE) = 0UL;
            enabled_ = false;
            started_ = false;
            ready_ = false;
            startupInProgress_ = false;
            resetConnectionState();
        }
        return;
    }

    if (reg32(USBD_BASE, EVENTS_USBRESET) != 0UL) {
        reg32(USBD_BASE, EVENTS_USBRESET) = 0UL;
        resetConnectionState();
    }

    if (reg32(USBD_BASE, EVENTS_STARTED) != 0UL) {
        reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
        started_ = true;
    }

    if (reg32(USBD_BASE, eventEndEpinOffset(0U)) != 0UL) {
        reg32(USBD_BASE, eventEndEpinOffset(0U)) = 0UL;
        completePendingAddress();
    }

    if (reg32(USBD_BASE, eventEndEpinOffset(SERVICE_NOTIFICATION_EP)) != 0UL) {
        reg32(USBD_BASE, eventEndEpinOffset(SERVICE_NOTIFICATION_EP)) = 0UL;
        // DMA-to-endpoint finished. Keep the notification busy until the host
        // ACK appears in EPDATASTATUS, otherwise a second notification can
        // overwrite the endpoint FIFO before usbser reads the first.
        diagResetAtUsbdBeginStage(21UL);
    }

    if (userPortEnabled() && reg32(USBD_BASE, eventEndEpinOffset(USER_NOTIFICATION_EP)) != 0UL) {
        reg32(USBD_BASE, eventEndEpinOffset(USER_NOTIFICATION_EP)) = 0UL;
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
            // zero length, and the exact seven-byte guard inside the
            // ServiceLineCoding case drops the entire SET_LINE_CODING payload — which
            // is precisely why the device's baud was stuck at its 115200 default and
            // the 1200 bps touch never armed.
            if (triggerEndpointStartTask(taskStartEpoutOffset(0U))) {
                completeControlOutTransfer();
                completePendingAddress();
            } else {
                // A bounded EasyDMA failure must not leave EP0 permanently
                // owned by a stale control-OUT transfer. Abort this request and
                // let the host recover through the normal control-pipe retry.
                pendingControlOut_ = ControlOutTransfer::None;
                controlOutExpected_ = 0U;
                controlOutLength_ = 0U;
                stallControlEndpoint();
            }
        }
        // Else: spurious EP0DATADONE with neither side tracked — drop silently.
    }

    // Finish an already-latched data stage before reading a newer SETUP token.
    // A host can advance to the next control request before this ISR/poll pass;
    // servicing SETUP first would replace our transfer state and either drop a
    // completed CDC line-coding payload or route its DONE event into the new IN
    // request. serviceSetup() then explicitly aborts only a genuinely
    // incomplete older transfer.
    if (reg32(USBD_BASE, EVENTS_EP0SETUP) != 0UL) {
        reg32(USBD_BASE, EVENTS_EP0SETUP) = 0UL;
        if (pollTraceEnabled()) {
            ++g_usbdPollTrace.ep0SetupEvents;
        }
        diagResetAtUsbdBeginStage(7UL);
        serviceSetup();
        diagResetAtUsbdBeginStage(8UL);
    }

    // A host OUT packet has been buffered when its EPDATASTATUS OUT bit is set
    // (EPOUT1..7 = bits 17..23); for each we serialize an EasyDMA STARTEPOUT,
    // then copy the completed packet into its software RX ring.
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
    // EPDATA is only an aggregate wake event. Its flag can be consumed just
    // before a per-endpoint ACK bit latches, so inspect the endpoint status on
    // every pass and recover a CDC IN transfer even when that aggregate edge
    // was missed during a bootloader-to-application handoff.
    uint32_t endpointStatus = reg32(USBD_BASE, EPDATASTATUS);
    {
        // On this silicon the aggregate EPDATA event can become visible a few
        // peripheral clocks before its endpoint-status bit. Briefly resample;
        // if it is still zero, leave EPDATA asserted so the IRQ is retriggered
        // instead of losing the only wake-up for the next queued IN packet.
        if (reg32(USBD_BASE, EVENTS_EPDATA) != 0UL && endpointStatus == 0UL) {
            for (uint32_t spin = 0UL; spin < 64UL && endpointStatus == 0UL; ++spin) {
                endpointStatus = reg32(USBD_BASE, EPDATASTATUS);
            }
        }
        const uint32_t serviceNotificationAck = 1UL << SERVICE_NOTIFICATION_EP;
        const uint32_t serviceInAck = 1UL << SERVICE_DATA_EP;
        const uint32_t userNotificationAck = 1UL << USER_NOTIFICATION_EP;
        const uint32_t userInAck = 1UL << USER_DATA_EP;
        uint32_t handledInAck = 0UL;
        if ((endpointStatus & serviceNotificationAck) != 0UL) {
            notificationInFlight_ = false;
            handledInAck |= serviceNotificationAck;
        }
        if ((endpointStatus & serviceInAck) != 0UL) {
            if (dataInFlight_) {
                txTail_ = (txTail_ + dataInFlightLength_) % USBD_RING_BUFFER_SIZE;
            }
            dataInFlightLength_ = 0U;
            dataInFlight_ = false;
            handledInAck |= serviceInAck;
        }
        if (userPortEnabled()) {
            if ((endpointStatus & userNotificationAck) != 0UL) {
                userNotificationInFlight_ = false;
                handledInAck |= userNotificationAck;
            }
            if ((endpointStatus & userInAck) != 0UL) {
                if (userDataInFlight_) {
                    userTxTail_ = (userTxTail_ + userDataInFlightLength_) % USBD_RING_BUFFER_SIZE;
                }
                userDataInFlightLength_ = 0U;
                userDataInFlight_ = false;
                handledInAck |= userInAck;
            }
        }
        for (uint8_t endpoint = firstDynamicEndpoint(); endpoint < USB_MAX_ENDPOINTS; ++endpoint) {
            const uint32_t dynamicInAck = 1UL << endpoint;
            if ((endpointStatus & dynamicInAck) != 0UL) {
                if (dynamicInBusy_[endpoint]) {
                    dynamicInBusy_[endpoint] = false;
                    dynamicInLengths_[endpoint] = 0U;
                    PluggableUSB().endpointInComplete(endpoint);
                }
                handledInAck |= dynamicInAck;
            }
        }
        if (handledInAck != 0UL) {
            reg32(USBD_BASE, EPDATASTATUS) = handledInAck;
        }
    }

    if (reg32(USBD_BASE, EVENTS_EPDATA) != 0UL) {
        reg32(USBD_BASE, EVENTS_EPDATA) = 0UL;
        // OUT packets remain pending until foreground pumpRx() or the halted
        // GDB loop has ring capacity and consumes the exact endpoint-status
        // bit. Keeping data reception out of this aggregate-event cleanup also
        // preserves backpressure and avoids competing EasyDMA launches.
    }

    if (reg32(USBD_BASE, eventEndEpoutOffset(SERVICE_DATA_EP)) != 0UL) {
        // OUT packets are consumed through their EPDATASTATUS bit by pumpRx().
        // The synchronous DMA scheduler deliberately leaves ENDEPOUT latched
        // for this common event-cleanup path; the bytes were already copied.
        reg32(USBD_BASE, eventEndEpoutOffset(SERVICE_DATA_EP)) = 0UL;
    }

    if (reg32(USBD_BASE, eventEndEpinOffset(SERVICE_DATA_EP)) != 0UL) {
        // DMA buffer->FIFO done; the host-read ACK (EPDATASTATUS.EPIN2 above)
        // is what frees us to send the next chunk, so don't clear dataInFlight_.
        reg32(USBD_BASE, eventEndEpinOffset(SERVICE_DATA_EP)) = 0UL;
        diagResetAtUsbdBeginStage(18UL);
    }

    if (userPortEnabled() && reg32(USBD_BASE, eventEndEpoutOffset(USER_DATA_EP)) != 0UL) {
        reg32(USBD_BASE, eventEndEpoutOffset(USER_DATA_EP)) = 0UL; // clear-only, see above
    }

    if (userPortEnabled() && reg32(USBD_BASE, eventEndEpinOffset(USER_DATA_EP)) != 0UL) {
        reg32(USBD_BASE, eventEndEpinOffset(USER_DATA_EP)) = 0UL;
    }

    for (uint8_t endpoint = firstDynamicEndpoint(); endpoint < USB_MAX_ENDPOINTS; ++endpoint) {
        if (reg32(USBD_BASE, eventEndEpinOffset(endpoint)) != 0UL) {
            reg32(USBD_BASE, eventEndEpinOffset(endpoint)) = 0UL;
        }
    }

    if (reg32(USBD_BASE, EVENTS_USBEVENT) != 0UL) {
        reg32(USBD_BASE, EVENTS_USBEVENT) = 0UL;
        eventCause_ = reg32(USBD_BASE, EVENTCAUSE);
        if (pollTraceEnabled()) {
            ++g_usbdPollTrace.usbEventEvents;
            g_usbdPollTrace.lastEventCause = eventCause_;
        }
        if ((eventCause_ & USBD_EVENTCAUSE_READY_MASK) != 0UL) {
            ready_ = attached_ && hasVbus;
        }
        if ((eventCause_ & USBD_EVENTCAUSE_SUSPEND_MASK) != 0UL) {
            suspended_ = true;
        }
        if ((eventCause_ & USBD_EVENTCAUSE_RESUME_MASK) != 0UL) {
            suspended_ = false;
            ready_ = attached_ && hasVbus;
        }
        reg32(USBD_BASE, EVENTCAUSE) = eventCause_;
    }
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
    return enabled_ && attached_ && ready_ && configured_ && !suspended_ &&
        effectiveVbusDetected() && dtr_ && dtrAssertedMillis_ != 0UL &&
        (millis() - dtrAssertedMillis_) >= USBD_CDC_OPEN_SETTLE_MS;
}

void NrfUsbdDriver::serviceTouchTimer() {
    if (!serviceTouchPending_) {
        return;
    }

    const bool resetArmed = configuredMillis_ != 0UL &&
        (millis() - configuredMillis_) >= USBD_1200_RESET_ARM_MS;
    const bool windowStarted = serviceTouchResetMillis_ != 0UL;
    const bool windowElapsed = windowStarted &&
        (millis() - serviceTouchResetMillis_) >= USBD_TOUCH_RESET_CONFIRM_MS;
    const bool inConfirmWindow = windowStarted && !windowElapsed;

    if (windowElapsed) {
        // The 1200-bps line coding plus DTR drop is already explicit host intent.
        // Do not re-check DTR: usbser may reassert it while closing the handle.
        serviceTouchPending_ = false;
        serviceTouchResetMillis_ = 0UL;
        if (ignoredResetTouchCount_ < USBD_IGNORE_INITIAL_1200_RESET_COUNT) {
            ++ignoredResetTouchCount_;
        } else {
            markResetCause(USBD_DIAG_CAUSE_1200_TOUCH);
            requestBootloaderReset();
        }
        return;
    }

    if (!configured_ || (dtr_ && !inConfirmWindow) ||
        lineCoding_.baudRate != 1200UL) {
        serviceTouchPending_ = false;
        serviceTouchResetMillis_ = 0UL;
    } else if (!resetArmed) {
        serviceTouchResetMillis_ = 0UL;
    } else if (!windowStarted) {
        serviceTouchResetMillis_ = millis();
    }
}

void NrfUsbdDriver::serviceTick() {
    if (!enabled_ || stubHalted_) {
        return;
    }
    serviceTouchTimer();
}

bool NrfUsbdDriver::userConnected() const {
    return enabled_ && attached_ && ready_ && configured_ && !suspended_ &&
        effectiveVbusDetected() && userPortEnabled() && userDtr_ &&
        userDtrAssertedMillis_ != 0UL &&
        (millis() - userDtrAssertedMillis_) >= USBD_CDC_OPEN_SETTLE_MS;
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
    UsbdIrqLock lock;
    processBusState(effectiveVbusDetected());
    if (!ringPushTx(value)) {
        return 0U;
    }
    if (configured_ && !suspended_) {
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

    // Recover any IN acknowledgement whose aggregate interrupt arrived before
    // EPDATASTATUS latched. Serial.available()/read() are foreground progress
    // points too, so a busy sketch cannot strand the following TX packet.
    processBusState(effectiveVbusDetected());

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
    // which clears dynamicInBusy_[endpoint] after the host's IN ACK.
    UsbdIrqLock lock;
    if (ep0InXferPhase_ != Ep0InXferPhase::Idle ||
        pendingControlOut_ != ControlOutTransfer::None) {
        return false;
    }
    if (dynamicInBusy_[endpoint]) {
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
    if (!triggerEndpointStartTask(taskStartEpinOffset(endpoint))) {
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
            if (!configured_ || suspended_ || !effectiveVbusDetected()) {
                break;
            }
            processBusState(effectiveVbusDetected());
            serviceDataIn(false);
        }
        return;
    }

    const uint32_t start = millis();
    while ((txPending() != 0U || dataInFlight_) && enabled_) {
        processBusState(effectiveVbusDetected());
        serviceDataIn(false);
        if (!configured_ || suspended_ || !effectiveVbusDetected()) {
            break;
        }
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
    if (dtr && !dtr_) {
        dtrAssertedMillis_ = millis();
    } else if (!dtr) {
        dtrAssertedMillis_ = 0UL;
    }
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
    UsbdIrqLock lock;
    processBusState(effectiveVbusDetected());
    if (!userPortEnabled() || !userRingPushTx(value)) {
        return 0U;
    }
    if (configured_ && !suspended_) {
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
    UsbdIrqLock lock;
    processBusState(effectiveVbusDetected());
    size_t written = 0U;
    while (written < length && userRingPushTx(data[written])) {
        ++written;
    }
    if (written > 0U && configured_ && !suspended_) {
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
        processBusState(effectiveVbusDetected());
        serviceDataIn(true);
        if (!configured_ || suspended_ || !effectiveVbusDetected()) {
            break;
        }
        if ((millis() - start) >= 50UL) {
            break;
        }
    }
}

void NrfUsbdDriver::setUserLineCoding(const NrfUsbLineCoding &lineCoding) {
    userLineCoding_ = lineCoding;
}

void NrfUsbdDriver::setUserLineState(bool dtr, bool rts) {
    if (dtr && !userDtr_) {
        userDtrAssertedMillis_ = millis();
    } else if (!dtr) {
        userDtrAssertedMillis_ = 0UL;
    }
    userDtr_ = dtr;
    userRts_ = rts;
}

NrfUsbdStatus NrfUsbdDriver::status() const {
    return {enabled_, started_, attached_, effectiveVbusDetected(), ready_, configured_, suspended_, cdcActive_, address_, configuration_, eventCause_};
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
    // ready_ is owned by begin() (set true) and end() (set false).
    configured_ = false;
    suspended_ = false;
    cdcActive_ = false;
    dataInFlight_ = false;
    dataInFlightLength_ = 0U;
    notificationInFlight_ = false;
    notificationPending_ = false;
    userDataInFlight_ = false;
    userDataInFlightLength_ = 0U;
    userNotificationInFlight_ = false;
    userNotificationPending_ = false;
    dtr_ = false;
    rts_ = false;
    dtrAssertedMillis_ = 0UL;
    userDtr_ = false;
    userRts_ = false;
    userDtrAssertedMillis_ = 0UL;
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
    haltedInEndpoints_ = 0U;
    haltedOutEndpoints_ = 0U;
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

bool NrfUsbdDriver::initDescriptors() {
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
    bool baseDescriptorValid = true;
    const auto appendDescriptor = [&](const uint8_t *data, size_t length) {
        if (length > (sizeof(configurationDescriptor_) - configurationDescriptorLength_)) {
            baseDescriptorValid = false;
            return;
        }
        for (size_t index = 0; index < length; ++index) {
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
    return baseDescriptorValid && configurationDescriptorLength_ >= sizeof(configHeader);
}

void NrfUsbdDriver::clearEvents() {
    reg32(USBD_BASE, EVENTS_USBRESET) = 0UL;
    reg32(USBD_BASE, EVENTS_STARTED) = 0UL;
    for (uint8_t endpoint = 0; endpoint < 8U; ++endpoint) {
        reg32(USBD_BASE, eventEndEpinOffset(endpoint)) = 0UL;
        reg32(USBD_BASE, eventEndEpoutOffset(endpoint)) = 0UL;
    }
    reg32(USBD_BASE, EVENTS_EP0DATADONE) = 0UL;
    reg32(USBD_BASE, EVENTS_USBEVENT) = 0UL;
    reg32(USBD_BASE, EVENTS_EP0SETUP) = 0UL;
    reg32(USBD_BASE, EVENTCAUSE) = 0xFFFFFFFFUL;
}

void NrfUsbdDriver::enableInterrupts() {
    const uint32_t interruptMask =
        USBD_INT_USBRESET_MASK |
        USBD_INT_STARTED_MASK |
        USBD_INT_ENDEPIN0_MASK |
        USBD_INT_ENDEPIN1_MASK |
        USBD_INT_ENDEPIN2_MASK |
        USBD_INT_ENDEPIN3_MASK |
        USBD_INT_ENDEPIN4_MASK |
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
}

void NrfUsbdDriver::disableInterrupts() {
    const uint32_t interruptMask =
        USBD_INT_USBRESET_MASK |
        USBD_INT_STARTED_MASK |
        USBD_INT_ENDEPIN0_MASK |
        USBD_INT_ENDEPIN1_MASK |
        USBD_INT_ENDEPIN2_MASK |
        USBD_INT_ENDEPIN3_MASK |
        USBD_INT_ENDEPIN4_MASK |
        USBD_INT_EP0DATADONE_MASK |
        USBD_INT_ENDEPOUT2_MASK |
        USBD_INT_ENDEPOUT4_MASK |
        USBD_INT_USBEVENT_MASK |
        USBD_INT_EP0SETUP_MASK |
        USBD_INT_EPDATA_MASK;
    reg32(USBD_BASE, INTENCLR) = interruptMask;
    disableNvicIrq(USBD_IRQ_NUMBER);
}

void NrfUsbdDriver::enablePullup(bool enabled) {
    if (enabled) {
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
    // A newly enabled OUT endpoint NAKs until software writes SIZE.EPOUT[n].
    // This write makes its peripheral buffer available; it does NOT start DMA.
    reg32(USBD_BASE, sizeEpoutOffset(SERVICE_DATA_EP)) = 0UL;
    if (userPortEnabled()) {
        reg32(USBD_BASE, EPINEN) |= endpointMask(USER_NOTIFICATION_EP) | endpointMask(USER_DATA_EP);
        reg32(USBD_BASE, EPOUTEN) |= endpointMask(USER_DATA_EP);
        resetEndpointDataState(epAddressIn(USER_NOTIFICATION_EP));
        resetEndpointDataState(epAddressIn(USER_DATA_EP));
        resetEndpointDataState(epAddressOut(USER_DATA_EP));
        reg32(USBD_BASE, sizeEpoutOffset(USER_DATA_EP)) = 0UL;
    }
    cdcActive_ = true;
}

void NrfUsbdDriver::fetchOutPacket(uint8_t endpoint, bool userPort, uint32_t statusBit) {
    // Called only when EPDATASTATUS.<statusBit> is already set: a COMPLETE host
    // packet is in this endpoint's FIFO (USB OUT is atomic). STARTEPOUT pulls it
    // into RAM AND re-arms the endpoint for the next OUT; we then consume the
    // data bit (W1C) and hand the bytes to the rx ring. Reading the bit BEFORE
    // (in drainServiceDataOut) avoids the race where a speculative STARTEPOUT on
    // an idle endpoint re-delivers stale FIFO content and shifts the RSP stream.
    uint8_t *buffer = userPort ? &userEndpointOutBuffer_[0] : &endpointOutBuffer_[0];
    uint32_t amount = reg32(USBD_BASE, sizeEpoutOffset(endpoint));
    if (amount > DATA_EP_MAX_PACKET) {
        amount = DATA_EP_MAX_PACKET;
    }
    reg32(USBD_BASE, epoutPtrOffset(endpoint)) = reinterpret_cast<uint32_t>(buffer);
    reg32(USBD_BASE, epoutMaxcntOffset(endpoint)) = amount;
    // Consume the current packet-ready indication BEFORE starting EasyDMA.
    // Once ENDEPOUT fires, the nRF controller may immediately ACK the next
    // host packet and re-latch this same bit. Clearing it after the DMA would
    // race that next packet and silently discard its only ready indication.
    // Nordic nrfx and TinyUSB both clear EPDATASTATUS before STARTEPOUT for
    // this reason.
    reg32(USBD_BASE, EPDATASTATUS) = statusBit;
    if (triggerEndpointStartTask(taskStartEpoutOffset(endpoint))) {
        if (amount != 0UL) {
            serviceDataOut(userPort, amount);
        }
        // After ENDEPOUT, hardware automatically accepts the next bulk OUT.
        // SIZE.EPOUT is written only when initially opening/resetting an OUT
        // endpoint. Writing it here can erase a packet accepted immediately
        // after the DMA, which appears as a one-byte loss in sustained duplex
        // streams.
    }
}

void NrfUsbdDriver::drainServiceDataOut() {
    if (!enabled_) {
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
    // Preserve USB backpressure. Fetching a packet acknowledges the endpoint;
    // if the software ring cannot hold the entire packet, the old code still
    // fetched it and silently dropped the tail. A host burst larger than the
    // 256-byte ring therefore truncated after a few packets. Leave the status
    // bit pending (and the endpoint NAKing the next packet) until the sketch has
    // drained at least one full maximum-size packet of space.
    const size_t serviceFree = (USBD_RING_BUFFER_SIZE - 1U) - ringPending(rxHead_, rxTail_);
    const size_t userFree = (USBD_RING_BUFFER_SIZE - 1U) - ringPending(userRxHead_, userRxTail_);
    if ((eds & serviceOutBit) != 0UL && serviceFree >= DATA_EP_MAX_PACKET) {
        fetchOutPacket(SERVICE_DATA_EP, false, serviceOutBit);
    }
    if (userPortEnabled() && (eds & userOutBit) != 0UL && userFree >= DATA_EP_MAX_PACKET) {
        fetchOutPacket(USER_DATA_EP, true, userOutBit);
    }
}

void NrfUsbdDriver::serviceDataOut(bool userPort, uint32_t received) {
    uint8_t *buffer = userPort ? &userEndpointOutBuffer_[0] : &endpointOutBuffer_[0];
    // NOTE: the caller passes the packet length read from SIZE.EPOUT[n]. The
    // EPOUT[n].AMOUNT register is NOT usable for this on the verified clone:
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
    // The next DMA starts only after EPDATASTATUS reports another complete OUT
    // packet. Never issue a speculative STARTEPOUT on an empty endpoint.
}

void NrfUsbdDriver::serviceDataIn(bool userPort) {
    if (suspended_ || ep0InXferPhase_ != Ep0InXferPhase::Idle ||
        pendingControlOut_ != ControlOutTransfer::None) {
        return;
    }
    volatile bool &dataInFlight = userPort ? userDataInFlight_ : dataInFlight_;
    volatile uint8_t &dataInFlightLength =
        userPort ? userDataInFlightLength_ : dataInFlightLength_;
    const bool dtr = userPort ? userDtr_ : dtr_;
    const uint32_t dtrAssertedMillis = userPort ? userDtrAssertedMillis_ : dtrAssertedMillis_;
    if (!dtr || dtrAssertedMillis == 0UL ||
        (millis() - dtrAssertedMillis) < USBD_CDC_OPEN_SETTLE_MS) {
        return;
    }
    if (!configured_ || dataInFlight || (userPort ? userTxPending() : txPending()) == 0U) {
        return;
    }

    uint8_t *txBuffer = userPort ? &userTxBuffer_[0] : &this->txBuffer_[0];
    volatile size_t &txHead = userPort ? userTxHead_ : txHead_;
    const size_t txTail = userPort ? userTxTail_ : txTail_;
    uint8_t *endpointBuffer = userPort ? &userEndpointInBuffer_[0] : &endpointInBuffer_[0];
    const uint8_t endpoint = userPort ? USER_DATA_EP : SERVICE_DATA_EP;
    size_t cursor = txTail;
    size_t count = 0U;
    while (count < DATA_EP_MAX_PACKET && cursor != txHead) {
        endpointBuffer[count++] = txBuffer[cursor];
        cursor = (cursor + 1U) % USBD_RING_BUFFER_SIZE;
    }

    if (count == 0U) {
        return;
    }

    if (!userPort) {
        diagResetAtUsbdBeginStage(16UL);
    }
    reg32(USBD_BASE, epinPtrOffset(endpoint)) = reinterpret_cast<uint32_t>(endpointBuffer);
    reg32(USBD_BASE, epinMaxcntOffset(endpoint)) = static_cast<uint32_t>(count);
    if (!triggerEndpointStartTask(taskStartEpinOffset(endpoint))) {
        return;
    }
    dataInFlightLength = static_cast<uint8_t>(count);
    dataInFlight = true;
    if (!userPort) {
        diagResetAtUsbdBeginStage(17UL);
    }
}

void NrfUsbdDriver::serviceNotificationIn(bool userPort) {
    if (suspended_ || ep0InXferPhase_ != Ep0InXferPhase::Idle ||
        pendingControlOut_ != ControlOutTransfer::None) {
        return;
    }
    volatile bool &notificationInFlight = userPort ? userNotificationInFlight_ : notificationInFlight_;
    volatile bool &notificationPending = userPort ? userNotificationPending_ : notificationPending_;
    if (!configured_ || notificationInFlight || !notificationPending) {
        return;
    }

    uint8_t *buffer = userPort ? &userNotificationBuffer_[0] : &notificationBuffer_[0];
    const uint8_t endpoint = userPort ? USER_NOTIFICATION_EP : SERVICE_NOTIFICATION_EP;
    reg32(USBD_BASE, epinPtrOffset(endpoint)) = reinterpret_cast<uint32_t>(buffer);
    reg32(USBD_BASE, epinMaxcntOffset(endpoint)) = 10UL;
    if (!userPort) {
        diagResetAtUsbdBeginStage(19UL);
    }
    if (!triggerEndpointStartTask(taskStartEpinOffset(endpoint))) {
        return;
    }
    notificationPending = false;
    notificationInFlight = true;
    if (!userPort) {
        diagResetAtUsbdBeginStage(20UL);
    }
}

void NrfUsbdDriver::serviceSetup() {
    // A SETUP token aborts any older control transfer. Do not let a short or
    // interrupted class OUT request retain ownership of EP0 and consume the
    // payload/status event belonging to the new request.
    pendingControlOut_ = ControlOutTransfer::None;
    controlOutExpected_ = 0U;
    controlOutLength_ = 0U;
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

    if (controlOutLength_ != controlOutExpected_) {
        // CDC line coding is a fixed-size request. A short/overlong data stage
        // is malformed and must not partially update state or receive a false
        // success status.
        pendingControlOut_ = ControlOutTransfer::None;
        controlOutExpected_ = 0U;
        controlOutLength_ = 0U;
        stallControlEndpoint();
        return;
    }

    switch (pendingControlOut_) {
        case ControlOutTransfer::ServiceLineCoding:
            if (controlOutLength_ == 7U) {
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
                    // Line coding alone is not an upload gesture. Windows can
                    // replay a port's saved 1200-baud setting while DTR is
                    // already low during enumeration. Arming here made that
                    // harmless replay turn into a delayed reboot and apparent
                    // COM-port loss. SET_CONTROL_LINE_STATE owns the explicit
                    // DTR high-to-low edge that authorizes bootloader entry.
                }
            }
            sendZeroLengthStatus();
            break;
        case ControlOutTransfer::UserLineCoding:
            if (controlOutLength_ == 7U) {
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

bool NrfUsbdDriver::interfaceExists(uint8_t interfaceNumber) const {
    size_t offset = 0U;
    while (offset + 2U <= configurationDescriptorLength_) {
        const uint8_t descriptorLength = configurationDescriptor_[offset];
        if (descriptorLength < 2U || offset + descriptorLength > configurationDescriptorLength_) {
            return false;
        }
        if (configurationDescriptor_[offset + 1U] == USB_DESC_INTERFACE &&
            descriptorLength >= 9U &&
            configurationDescriptor_[offset + 2U] == interfaceNumber) {
            return true;
        }
        offset += descriptorLength;
    }
    return false;
}

bool NrfUsbdDriver::endpointDescriptorAttributes(uint8_t endpointAddress, uint8_t &attributes) const {
    size_t offset = 0U;
    while (offset + 2U <= configurationDescriptorLength_) {
        const uint8_t descriptorLength = configurationDescriptor_[offset];
        if (descriptorLength < 2U || offset + descriptorLength > configurationDescriptorLength_) {
            return false;
        }
        if (configurationDescriptor_[offset + 1U] == USB_DESC_ENDPOINT &&
            descriptorLength >= 7U &&
            configurationDescriptor_[offset + 2U] == endpointAddress) {
            attributes = configurationDescriptor_[offset + 3U];
            return true;
        }
        offset += descriptorLength;
    }
    return false;
}

bool NrfUsbdDriver::endpointRequestValid(uint16_t index,
                                         uint8_t &endpointAddress,
                                         uint8_t &attributes) const {
    if ((index & 0xFF70U) != 0U) {
        return false;
    }
    endpointAddress = static_cast<uint8_t>(index & 0x008FU);
    const uint8_t endpoint = static_cast<uint8_t>(endpointAddress & 0x0FU);
    if (!configured_ || endpoint == 0U || endpoint >= USB_MAX_ENDPOINTS) {
        return false;
    }
    return endpointDescriptorAttributes(endpointAddress, attributes) &&
           (attributes & 0x03U) != 0x01U;  // ENDPOINT_HALT is invalid for isochronous endpoints.
}

void NrfUsbdDriver::setEndpointHalt(uint8_t endpointAddress, bool halted) {
    const uint8_t endpoint = static_cast<uint8_t>(endpointAddress & 0x0FU);
    volatile uint8_t &bitmap = (endpointAddress & USB_DIR_IN) != 0U
        ? haltedInEndpoints_
        : haltedOutEndpoints_;
    if (halted) {
        stallEndpoint(endpointAddress);
        bitmap = static_cast<uint8_t>(bitmap | endpointMask(endpoint));
    } else {
        // If the host ACKed the last IN packet immediately before issuing
        // CLEAR_FEATURE, commit that acknowledgement before resetting the
        // endpoint. Otherwise preserve queued CDC bytes for retransmission.
        const uint32_t ackBit = endpointMask(endpoint);
        const bool inEndpoint = (endpointAddress & USB_DIR_IN) != 0U;
        const bool acked = inEndpoint && (reg32(USBD_BASE, EPDATASTATUS) & ackBit) != 0U;
        if (acked) {
            if (endpoint == SERVICE_DATA_EP && dataInFlight_) {
                txTail_ = (txTail_ + dataInFlightLength_) % USBD_RING_BUFFER_SIZE;
            } else if (userPortEnabled() && endpoint == USER_DATA_EP && userDataInFlight_) {
                userTxTail_ = (userTxTail_ + userDataInFlightLength_) % USBD_RING_BUFFER_SIZE;
            } else if (endpoint >= firstDynamicEndpoint() && dynamicInBusy_[endpoint]) {
                PluggableUSB().endpointInComplete(endpoint);
            }
            reg32(USBD_BASE, EPDATASTATUS) = ackBit;
        }

        if (inEndpoint && endpoint == SERVICE_DATA_EP) {
            dataInFlight_ = false;
            dataInFlightLength_ = 0U;
        } else if (inEndpoint && endpoint == SERVICE_NOTIFICATION_EP) {
            notificationInFlight_ = false;
        } else if (inEndpoint && userPortEnabled() && endpoint == USER_DATA_EP) {
            userDataInFlight_ = false;
            userDataInFlightLength_ = 0U;
        } else if (inEndpoint && userPortEnabled() && endpoint == USER_NOTIFICATION_EP) {
            userNotificationInFlight_ = false;
        } else if (inEndpoint && endpoint >= firstDynamicEndpoint()) {
            dynamicInBusy_[endpoint] = false;
            dynamicInLengths_[endpoint] = 0U;
        }
        clearEndpointStall(endpointAddress);
        clearEndpointToggle(endpointAddress);
        bitmap = static_cast<uint8_t>(bitmap & ~endpointMask(endpoint));
    }
}

bool NrfUsbdDriver::endpointHalted(uint8_t endpointAddress) const {
    const uint8_t endpoint = static_cast<uint8_t>(endpointAddress & 0x0FU);
    const uint8_t bitmap = (endpointAddress & USB_DIR_IN) != 0U
        ? haltedInEndpoints_
        : haltedOutEndpoints_;
    return (bitmap & endpointMask(endpoint)) != 0U;
}

void NrfUsbdDriver::handleStandardRequest(uint8_t request, uint16_t value, uint16_t index, uint16_t length) {
    const uint8_t requestType = static_cast<uint8_t>(reg32(USBD_BASE, BMREQUESTTYPE) & 0xFFUL);
    const uint8_t recipient = static_cast<uint8_t>(reg32(USBD_BASE, BMREQUESTTYPE) & 0x1FU);

    switch (request) {
        case USB_REQ_GET_DESCRIPTOR: {
            const uint8_t descriptorType = static_cast<uint8_t>((value >> 8U) & 0xFFU);
            const uint8_t descriptorIndex = static_cast<uint8_t>(value & 0xFFU);
            if (descriptorType == USB_DESC_DEVICE) {
                if (requestType != (USB_DIR_IN | USB_REQ_RECIPIENT_DEVICE) ||
                    descriptorIndex != 0U || index != 0U) {
                    stallControlEndpoint();
                    return;
                }
                size_t descriptorLength = length;
                if (descriptorLength > sizeof(deviceDescriptor_)) {
                    descriptorLength = sizeof(deviceDescriptor_);
                }
                startControlIn(deviceDescriptor_, descriptorLength);
                return;
            }
            if (descriptorType == USB_DESC_CONFIGURATION) {
                if (requestType != (USB_DIR_IN | USB_REQ_RECIPIENT_DEVICE) ||
                    descriptorIndex != 0U || index != 0U) {
                    stallControlEndpoint();
                    return;
                }
                size_t descriptorLength = length;
                if (descriptorLength > configurationDescriptorLength_) {
                    descriptorLength = configurationDescriptorLength_;
                }
                startControlIn(configurationDescriptor_, descriptorLength);
                return;
            }
            if (descriptorType == USB_DESC_STRING) {
                if (requestType != (USB_DIR_IN | USB_REQ_RECIPIENT_DEVICE) ||
                    (descriptorIndex == 0U ? index != 0U : (index != 0U && index != 0x0409U))) {
                    stallControlEndpoint();
                    return;
                }
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
#if !defined(NRF_USB_RUNTIME_DISABLE_APP_DFU) || (NRF_USB_RUNTIME_DISABLE_APP_DFU == 0)
                    text = "Nius Bootloader Control";
#endif
                } else {
                    stallControlEndpoint();
                    return;
                }
                if (text == nullptr ||
                    ((!userPortEnabled()) &&
                     (descriptorIndex == USB_STRING_USER_CONTROL || descriptorIndex == USB_STRING_USER_DATA))) {
                    stallControlEndpoint();
                    return;
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
            if (requestType != (USB_DIR_OUT | USB_REQ_RECIPIENT_DEVICE) ||
                value > 127U || index != 0U || length != 0U || configured_) {
                stallControlEndpoint();
                return;
            }
            pendingAddress_ = static_cast<uint8_t>(value);
            pendingAddressValid_ = true;
            diagResetAtUsbdBeginStage(9UL);
            sendZeroLengthStatus();
            return;
        case USB_REQ_SET_CONFIGURATION:
            if (requestType != (USB_DIR_OUT | USB_REQ_RECIPIENT_DEVICE) ||
                value > 1U || index != 0U || length != 0U) {
                stallControlEndpoint();
                return;
            }
            configuration_ = static_cast<uint8_t>(value);
            configured_ = configuration_ != 0U;
            if (configured_) {
                // SET_CONFIGURATION establishes a fresh endpoint state even
                // when the host selects configuration 1 repeatedly.
                reg32(USBD_BASE, EPINEN) = endpointMask(0U);
                reg32(USBD_BASE, EPOUTEN) = endpointMask(0U);
                haltedInEndpoints_ = 0U;
                haltedOutEndpoints_ = 0U;
                dataInFlight_ = false;
                dataInFlightLength_ = 0U;
                notificationInFlight_ = false;
                notificationPending_ = false;
                userDataInFlight_ = false;
                userDataInFlightLength_ = 0U;
                userNotificationInFlight_ = false;
                userNotificationPending_ = false;
                resetDynamicEndpoints();
                configuredMillis_ = millis();
                startCdcEndpoints();
            } else {
                configuredMillis_ = 0UL;
                cdcActive_ = false;
                dataInFlight_ = false;
                dataInFlightLength_ = 0U;
                notificationInFlight_ = false;
                notificationPending_ = false;
                userDataInFlight_ = false;
                userDataInFlightLength_ = 0U;
                userNotificationInFlight_ = false;
                userNotificationPending_ = false;
                dtr_ = false;
                rts_ = false;
                dtrAssertedMillis_ = 0U;
                userDtr_ = false;
                userRts_ = false;
                userDtrAssertedMillis_ = 0U;
                serviceTouchPending_ = false;
                serviceTouchResetMillis_ = 0U;
                haltedInEndpoints_ = 0U;
                haltedOutEndpoints_ = 0U;
                reg32(USBD_BASE, EPINEN) = endpointMask(0U);
                reg32(USBD_BASE, EPOUTEN) = endpointMask(0U);
                resetDynamicEndpoints();
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
            if (requestType != (USB_DIR_IN | USB_REQ_RECIPIENT_DEVICE) ||
                value != 0U || index != 0U || length != 1U) {
                stallControlEndpoint();
                return;
            }
            controlInBuffer_[0] = configuration_;
            startControlIn(controlInBuffer_, 1U);
            return;
        case USB_REQ_GET_STATUS: {
            if ((requestType & USB_DIR_IN) == 0U || value != 0U || length != 2U) {
                stallControlEndpoint();
                return;
            }
            controlInBuffer_[0] = 0U;
            controlInBuffer_[1] = 0U;
            if (recipient == USB_REQ_RECIPIENT_DEVICE) {
                if (requestType != (USB_DIR_IN | USB_REQ_RECIPIENT_DEVICE) || index != 0U) {
                    stallControlEndpoint();
                    return;
                }
            } else if (recipient == USB_REQ_RECIPIENT_INTERFACE) {
                if (requestType != (USB_DIR_IN | USB_REQ_RECIPIENT_INTERFACE) ||
                    !configured_ || (index & 0xFF00U) != 0U ||
                    !interfaceExists(static_cast<uint8_t>(index))) {
                    stallControlEndpoint();
                    return;
                }
            } else if (recipient == USB_REQ_RECIPIENT_ENDPOINT) {
                if (requestType != (USB_DIR_IN | USB_REQ_RECIPIENT_ENDPOINT) ||
                    (index & 0xFF70U) != 0U) {
                    stallControlEndpoint();
                    return;
                }
                const uint8_t endpointAddress = static_cast<uint8_t>(index & 0x008FU);
                const uint8_t endpoint = static_cast<uint8_t>(endpointAddress & 0x0FU);
                uint8_t attributes = 0U;
                if (endpoint >= USB_MAX_ENDPOINTS ||
                    (endpoint != 0U &&
                     (!configured_ || !endpointDescriptorAttributes(endpointAddress, attributes)))) {
                    stallControlEndpoint();
                    return;
                }
                controlInBuffer_[0] = endpointHalted(endpointAddress) ? 1U : 0U;
            } else {
                stallControlEndpoint();
                return;
            }
            startControlIn(controlInBuffer_, 2U);
            return;
        }
        case USB_REQ_CLEAR_FEATURE:
        case USB_REQ_SET_FEATURE: {
            if (requestType != (USB_DIR_OUT | USB_REQ_RECIPIENT_ENDPOINT) ||
                value != USB_FEATURE_ENDPOINT_HALT || length != 0U) {
                stallControlEndpoint();
                return;
            }
            uint8_t endpointAddress = 0U;
            uint8_t attributes = 0U;
            if (!endpointRequestValid(index, endpointAddress, attributes)) {
                stallControlEndpoint();
                return;
            }
            setEndpointHalt(endpointAddress, request == USB_REQ_SET_FEATURE);
            sendZeroLengthStatus();
            return;
        }
        case USB_REQ_GET_INTERFACE:
            if (requestType != (USB_DIR_IN | USB_REQ_RECIPIENT_INTERFACE) ||
                value != 0U || length != 1U || !configured_ ||
                (index & 0xFF00U) != 0U || !interfaceExists(static_cast<uint8_t>(index))) {
                stallControlEndpoint();
                return;
            }
            controlInBuffer_[0] = 0U;
            startControlIn(controlInBuffer_, 1U);
            return;
        case USB_REQ_SET_INTERFACE:
            if (requestType != (USB_DIR_OUT | USB_REQ_RECIPIENT_INTERFACE) ||
                value != 0U || length != 0U || !configured_ ||
                (index & 0xFF00U) != 0U || !interfaceExists(static_cast<uint8_t>(index))) {
                stallControlEndpoint();
                return;
            }
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
    if (recipient != USB_REQ_RECIPIENT_INTERFACE || !configured_) {
        stallControlEndpoint();
        return;
    }

    if ((index & 0xFF00U) != 0U) {
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
                if (requestType != (USB_DIR_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE) ||
                    value != 0U || length != 7U) {
                    stallControlEndpoint();
                    return;
                }
                expectControlOut(userCdc ? ControlOutTransfer::UserLineCoding : ControlOutTransfer::ServiceLineCoding, 7U);
                diagResetAtUsbdBeginStage(13UL);
                return;
            case CDC_REQ_GET_LINE_CODING:
                if (requestType != (USB_DIR_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE) ||
                    value != 0U || length != 7U) {
                    stallControlEndpoint();
                    return;
                }
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
                if (requestType != (USB_DIR_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE) ||
                    (value & 0xFFFCU) != 0U || length != 0U) {
                    stallControlEndpoint();
                    return;
                }
                {
                    const bool nextDtr = (value & 0x0001U) != 0U;
                    const bool previousDtr = dtr;
                    volatile uint32_t &dtrAssertedMillis =
                        userCdc ? userDtrAssertedMillis_ : dtrAssertedMillis_;
                    if (nextDtr && !dtr) {
                        dtrAssertedMillis = millis();
                    } else if (!nextDtr) {
                        dtrAssertedMillis = 0UL;
                    }
                    dtr = nextDtr;
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
                    if (!userCdc && recent1200) {
                        if (explicitTouchGesture(previousDtr, nextDtr, recent1200, resetArmed)) {
                            // Host dropped DTR at 1200 baud: arm the touch and start the 40 ms
                            // confirm timer (see USBD_TOUCH_RESET_CONFIRM_MS). The poll/IRQ gate
                            // confirms by writing the GPREGRET magic and triggering SYSRESETREQ.
                            markResetCauseIfUnset(USBD_DIAG_CAUSE_1200_TOUCH_PENDING);
                            serviceTouchPending_ = true;
                            serviceTouchResetMillis_ = millis();
                        } else if (serviceTouchResetMillis_ == 0UL) {
                            // V1 latch fix: once the confirm window has started
                            // (serviceTouchResetMillis_ != 0) we must NOT cancel the pending
                            // touch on a subsequent DTR=true. Windows usbser.sys re-asserts
                            // DTR on SerialPort.Close() and again on the next CreateFile()
                            // (adafruit-nrfutil opens the port immediately after the touch),
                            // so the host-side touch sequence inevitably ends with DTR back
                            // high within tens of ms. Cancelling here would defeat the touch.
                            serviceTouchPending_ = false;
                            serviceTouchResetMillis_ = 0UL;
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
                if (requestType != (USB_DIR_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE) ||
                    length != 0U) {
                    stallControlEndpoint();
                    return;
                }
                detachCause_ = USBD_DIAG_CAUSE_DFU_DETACH;
                detachRequestMagic_ = USBD_DETACH_REQUEST_MAGIC;
                sendZeroLengthStatus();
                return;
            case DFU_REQ_GETSTATUS:
                if (requestType != (USB_DIR_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE) ||
                    value != 0U || length != 6U) {
                    stallControlEndpoint();
                    return;
                }
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
                if (requestType != (USB_DIR_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE) ||
                    value != 0U || length != 1U) {
                    stallControlEndpoint();
                    return;
                }
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
        if (!triggerEndpointStartTask(taskStartEpinOffset(0U))) {
            resetEp0InXferState();
            stallControlEndpoint();
        }
        return;
    }
    if (ep0InNeedsZlp_) {
        ep0InNeedsZlp_ = false;
        reg32(USBD_BASE, epinPtrOffset(0U)) = reinterpret_cast<uint32_t>(&controlInBuffer_[0]);
        reg32(USBD_BASE, epinMaxcntOffset(0U)) = 0UL;
        if (!triggerEndpointStartTask(taskStartEpinOffset(0U))) {
            resetEp0InXferState();
            stallControlEndpoint();
        }
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
    // EP0STATUS is a peripheral handshake, not an EasyDMA transfer. There is
    // no second EP0DATADONE after this task on nRF52840, so retaining
    // StatusPending would permanently block every non-control endpoint once
    // EP0 serialization is enabled.
    ep0InXferPhase_ = Ep0InXferPhase::Idle;
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
    // SET_ADDRESS is applied by nRF USBD hardware after the status stage;
    // USBADDR is read-only. Keep only the software mirror used by status().
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
