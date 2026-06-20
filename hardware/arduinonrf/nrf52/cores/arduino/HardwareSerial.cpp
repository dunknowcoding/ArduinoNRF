#include "Arduino.h"
#include "HardwareSerial.h"
#include "NrfSystem.h"
#include "NrfUsbSerial.h"
#include "NrfUsbd.h"

namespace {

// When the boards menu disables the user CDC, the USB descriptor exposes only
// the fixed service/debug interface pair. Route Arduino `Serial` there so
// sketches and the IDE serial monitor keep working on a single cable.
inline bool nrfSerialUsbUsesServicePortOnly() {
    return nrfUsbRuntimeEnabled() && !nrfUsbUserPortEnabled();
}
// Serial1 is driven by UARTE0 (EasyDMA), NOT the legacy UART. UARTE and the
// legacy UART are two register views of the SAME peripheral instance (0x40002000)
// and must never be enabled at the same time; this core uses UARTE exclusively,
// so there is no UART/UARTE conflict. RX is interrupt-driven into a ring buffer,
// so bytes are no longer dropped between read() calls the way the old polled
// legacy-UART path could.
constexpr uint32_t UARTE0_BASE = 0x40002000UL;
constexpr uint32_t NRF_PSEL_DISCONNECTED = 0xFFFFFFFFUL;
constexpr uint32_t UARTE_TASKS_STARTRX = 0x000UL;
constexpr uint32_t UARTE_TASKS_STOPRX  = 0x004UL;
constexpr uint32_t UARTE_TASKS_STARTTX = 0x008UL;
constexpr uint32_t UARTE_TASKS_STOPTX  = 0x00CUL;
constexpr uint32_t UARTE_EVENTS_ENDRX  = 0x110UL;
constexpr uint32_t UARTE_EVENTS_ENDTX  = 0x120UL;
constexpr uint32_t UARTE_SHORTS    = 0x200UL;
constexpr uint32_t UARTE_INTENSET  = 0x304UL;
constexpr uint32_t UARTE_INTENCLR  = 0x308UL;
constexpr uint32_t UARTE_ENABLE    = 0x500UL;
constexpr uint32_t UARTE_PSELRTS   = 0x508UL;
constexpr uint32_t UARTE_PSELTXD   = 0x50CUL;
constexpr uint32_t UARTE_PSELCTS   = 0x510UL;
constexpr uint32_t UARTE_PSELRXD   = 0x514UL;
constexpr uint32_t UARTE_RXD_PTR    = 0x534UL;
constexpr uint32_t UARTE_RXD_MAXCNT = 0x538UL;
constexpr uint32_t UARTE_TXD_PTR    = 0x544UL;
constexpr uint32_t UARTE_TXD_MAXCNT = 0x548UL;
constexpr uint32_t UARTE_BAUDRATE  = 0x524UL;
constexpr uint32_t UARTE_CONFIG    = 0x56CUL;
constexpr uint32_t UARTE_ENABLE_DISABLED = 0UL;
constexpr uint32_t UARTE_ENABLE_ENABLED  = 8UL;   // 8 = UARTE (4 would be legacy UART)
constexpr uint32_t UARTE_SHORTS_ENDRX_STARTRX = (1UL << 5);
constexpr uint32_t UARTE_INT_ENDRX = (1UL << 4);
constexpr uint32_t UARTE0_IRQ_NUMBER = 2UL;       // UARTE0_UART0
constexpr uint32_t UART_TIMEOUT_SPINS = 200000UL;

inline void nvicEnableIrq(uint32_t irq) {
    *reinterpret_cast<volatile uint32_t *>(0xE000E100UL + (irq / 32UL) * 4UL) = 1UL << (irq % 32UL);
}
inline void nvicDisableIrq(uint32_t irq) {
    *reinterpret_cast<volatile uint32_t *>(0xE000E180UL + (irq / 32UL) * 4UL) = 1UL << (irq % 32UL);
}

// UARTE0 RX state: a 1-byte EasyDMA target plus a software ring the ISR fills.
// Do not use the ENDRX_STARTRX short with this single-byte target: the next byte
// can overwrite g_uarteRxByte before the ISR copies it, corrupting short framed
// UART replies. Instead the ISR copies first, then explicitly re-arms RX.
constexpr uint16_t UARTE_RX_RING_SIZE = 256;
volatile uint8_t  g_uarteRxRing[UARTE_RX_RING_SIZE];
volatile uint16_t g_uarteRxHead = 0;   // advanced by the ISR (producer)
volatile uint16_t g_uarteRxTail = 0;   // advanced by read() (consumer)
uint8_t  g_uarteRxByte = 0;            // EasyDMA RX target (must live in RAM)
uint8_t  g_uarteTxByte = 0;            // EasyDMA TX source (must live in RAM)

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline uint32_t rawPinFromBoardPin(uint8_t boardPin) {
    if (boardPin < PINS_COUNT) {
        return g_ADigitalPinMap[boardPin];
    }
    return NRF_PSEL_DISCONNECTED;
}

inline uint32_t baudRegisterValue(unsigned long baudRate) {
    if (baudRate >= 1000000UL) {
        return 0x10000000UL;
    }
    if (baudRate >= 460800UL) {
        return 0x07400000UL;
    }
    if (baudRate >= 230400UL) {
        return 0x03B00000UL;
    }
    if (baudRate >= 115200UL) {
        return 0x01D7E000UL;
    }
    if (baudRate >= 57600UL) {
        return 0x00EBF000UL;
    }
    if (baudRate >= 38400UL) {
        return 0x009D5000UL;
    }
    if (baudRate >= 19200UL) {
        return 0x004EA000UL;
    }
    return 0x00275000UL;
}
}

HardwareSerial Serial(true);
HardwareSerial Serial1(false);

HardwareSerial::HardwareSerial(bool usbBacked)
    : baudRate_(0), usbBacked_(usbBacked), enabled_(false) {
}

void HardwareSerial::begin(unsigned long baudRate) {
    baudRate_ = baudRate;
    if (!usbBacked_) {
        configureUart();
    } else {
        unsigned long actualBaudRate = baudRate;
        if (actualBaudRate == 0UL) {
            actualBaudRate = 115200UL;
        }
        nrfUsbSerialBackend().begin(actualBaudRate);
        if (nrfSerialUsbUsesServicePortOnly()) {
            NrfUsbLineCoding lc = nrfUsbdDriver().lineCoding();
            lc.baudRate = static_cast<uint32_t>(actualBaudRate);
            nrfUsbdDriver().setLineCoding(lc);
        }
        enabled_ = true;
    }
}

void HardwareSerial::begin(unsigned long baudRate, uint16_t config) {
    (void)config;
    begin(baudRate);
}

void HardwareSerial::end(void) {
    if (!usbBacked_) {
        nvicDisableIrq(UARTE0_IRQ_NUMBER);
        reg32(UARTE0_BASE, UARTE_INTENCLR) = 0xFFFFFFFFUL;
        reg32(UARTE0_BASE, UARTE_SHORTS) = 0UL;
        reg32(UARTE0_BASE, UARTE_TASKS_STOPRX) = 1UL;
        reg32(UARTE0_BASE, UARTE_TASKS_STOPTX) = 1UL;
        reg32(UARTE0_BASE, UARTE_ENABLE) = UARTE_ENABLE_DISABLED;
        reg32(UARTE0_BASE, UARTE_PSELTXD) = NRF_PSEL_DISCONNECTED;
        reg32(UARTE0_BASE, UARTE_PSELRXD) = NRF_PSEL_DISCONNECTED;
        reg32(UARTE0_BASE, UARTE_PSELRTS) = NRF_PSEL_DISCONNECTED;
        reg32(UARTE0_BASE, UARTE_PSELCTS) = NRF_PSEL_DISCONNECTED;
    } else {
        nrfUsbSerialBackend().end();
    }
    baudRate_ = 0;
    enabled_ = false;
}

int HardwareSerial::available(void) {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            nrfUsbdDriver().pumpRx(); // fetch any host OUT packet the ISR missed
            return nrfUsbdDriver().available();
        }
        return nrfUsbSerialBackend().available();
    }
    return static_cast<int>((g_uarteRxHead - g_uarteRxTail) & (UARTE_RX_RING_SIZE - 1));
}

int HardwareSerial::read(void) {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            nrfUsbdDriver().pumpRx();
            return nrfUsbdDriver().read();
        }
        return nrfUsbSerialBackend().read();
    }
    if (g_uarteRxHead == g_uarteRxTail) {
        return -1;
    }
    uint8_t value = g_uarteRxRing[g_uarteRxTail];
    g_uarteRxTail = (g_uarteRxTail + 1) & (UARTE_RX_RING_SIZE - 1);
    return value;
}

int HardwareSerial::peek(void) {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            nrfUsbdDriver().pumpRx();
            return nrfUsbdDriver().peek();
        }
        return nrfUsbSerialBackend().peek();
    }
    if (g_uarteRxHead == g_uarteRxTail) {
        return -1;
    }
    return g_uarteRxRing[g_uarteRxTail];
}

void HardwareSerial::flush(void) {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            nrfUsbdDriver().flush();
        } else {
            nrfUsbSerialBackend().flush();
        }
        return;
    }

    if (!enabled_) {
        return;
    }

    // write() blocks until each byte's ENDTX, so nothing is in flight here.
    for (uint32_t spin = 0; spin < UART_TIMEOUT_SPINS; ++spin) {
        if (reg32(UARTE0_BASE, UARTE_EVENTS_ENDTX) != 0UL) {
            break;
        }
    }
}

size_t HardwareSerial::write(uint8_t value) {
    if (usbBacked_) {
        enabled_ = true;
        if (nrfSerialUsbUsesServicePortOnly()) {
            return nrfUsbdDriver().write(value);
        }
        return nrfUsbSerialBackend().write(value);
    }

    if (!enabled_) {
        unsigned long actualBaudRate = baudRate_;
        if (actualBaudRate == 0UL) {
            actualBaudRate = 115200UL;
        }
        begin(actualBaudRate);
    }

    // Send one byte via EasyDMA: stage it in RAM, point TXD at it, MAXCNT=1,
    // STARTTX, then wait for ENDTX. (TX and RX use separate DMA, so this does
    // not disturb the continuous RX.)
    g_uarteTxByte = value;
    reg32(UARTE0_BASE, UARTE_TXD_PTR) = reinterpret_cast<uint32_t>(&g_uarteTxByte);
    reg32(UARTE0_BASE, UARTE_TXD_MAXCNT) = 1UL;
    reg32(UARTE0_BASE, UARTE_EVENTS_ENDTX) = 0UL;
    reg32(UARTE0_BASE, UARTE_TASKS_STARTTX) = 1UL;
    for (uint32_t spin = 0; spin < UART_TIMEOUT_SPINS; ++spin) {
        if (reg32(UARTE0_BASE, UARTE_EVENTS_ENDTX) != 0UL) {
            break;
        }
    }
    reg32(UARTE0_BASE, UARTE_TASKS_STOPTX) = 1UL;
    return 1;
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size) {
    if (buffer == nullptr || size == 0U) {
        return 0U;
    }
    // Fast path for the user CDC (the throughput-critical port): push the whole
    // buffer in one block so the IN endpoint is armed once, not once per byte.
    if (usbBacked_ && !nrfSerialUsbUsesServicePortOnly()) {
        enabled_ = true;
        return nrfUsbSerialBackend().write(buffer, size);
    }
    // Service-CDC-only USB and the UARTE path keep the simple per-byte behavior.
    size_t n = 0U;
    while (n < size) {
        if (write(buffer[n]) != 1U) {
            break;
        }
        ++n;
    }
    return n;
}

int HardwareSerial::availableForWrite() {
    if (!enabled_ && !usbBacked_) {
        return 0;
    }

    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            return nrfUsbdDriver().configured() ? 256 : 0;
        }
        const size_t pending = nrfUsbSerialBackend().txPending();
        const size_t bufferLimit = NrfUsbSerialBackend::BufferSize;
        if (pending < bufferLimit) {
            return static_cast<int>(bufferLimit - pending);
        }
        return 0;
    }

    return 1;
}

HardwareSerial::operator bool() const {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            return nrfUsbdDriver().configured();
        }
        return connected();
    }
    return enabled_;
}

bool HardwareSerial::isUsbBacked(void) const {
    return usbBacked_;
}

bool HardwareSerial::connected(void) const {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            return nrfUsbdDriver().connected();
        }
        return nrfUsbSerialBackend().connected();
    }
    return enabled_;
}

unsigned long HardwareSerial::baud(void) const {
    if (usbBacked_ && enabled_ && nrfSerialUsbUsesServicePortOnly()) {
        return nrfUsbdDriver().baud();
    }
    return baudRate_;
}

unsigned long HardwareSerial::baudRate(void) const {
    return baud();
}

bool HardwareSerial::dtr() const {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            return nrfUsbdDriver().dtr();
        }
        return nrfUsbSerialBackend().dtr();
    }
    return false;
}

bool HardwareSerial::rts() const {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            return nrfUsbdDriver().rts();
        }
        return nrfUsbSerialBackend().rts();
    }
    return false;
}

void HardwareSerial::configureUart() {
    const uint32_t rawTx = rawPinFromBoardPin(PIN_SERIAL_TX);
    const uint32_t rawRx = rawPinFromBoardPin(PIN_SERIAL_RX);
    if (rawTx == NRF_PSEL_DISCONNECTED || rawRx == NRF_PSEL_DISCONNECTED) {
        enabled_ = false;
        return;
    }

    pinMode(PIN_SERIAL_TX, OUTPUT);
    digitalWrite(PIN_SERIAL_TX, HIGH);     // UART line idles high
    pinMode(PIN_SERIAL_RX, INPUT_PULLUP);

    reg32(UARTE0_BASE, UARTE_ENABLE) = UARTE_ENABLE_DISABLED;
    reg32(UARTE0_BASE, UARTE_PSELTXD) = rawTx;
    reg32(UARTE0_BASE, UARTE_PSELRXD) = rawRx;
    reg32(UARTE0_BASE, UARTE_PSELRTS) = NRF_PSEL_DISCONNECTED;
    reg32(UARTE0_BASE, UARTE_PSELCTS) = NRF_PSEL_DISCONNECTED;
    reg32(UARTE0_BASE, UARTE_CONFIG) = 0UL;     // 8 data bits, no parity, 1 stop, no HW flow
    unsigned long actualBaudRate = baudRate_;
    if (actualBaudRate == 0UL) {
        actualBaudRate = 115200UL;
    }
    reg32(UARTE0_BASE, UARTE_BAUDRATE) = baudRegisterValue(actualBaudRate);

    // Continuous 1-byte DMA RX: point RXD at our byte, MAXCNT=1. The ENDRX
    // interrupt copies the byte into the ring and then re-arms RX.
    g_uarteRxHead = 0;
    g_uarteRxTail = 0;
    reg32(UARTE0_BASE, UARTE_RXD_PTR) = reinterpret_cast<uint32_t>(&g_uarteRxByte);
    reg32(UARTE0_BASE, UARTE_RXD_MAXCNT) = 1UL;
    reg32(UARTE0_BASE, UARTE_SHORTS) = 0UL;
    reg32(UARTE0_BASE, UARTE_EVENTS_ENDRX) = 0UL;
    reg32(UARTE0_BASE, UARTE_INTENCLR) = 0xFFFFFFFFUL;
    reg32(UARTE0_BASE, UARTE_INTENSET) = UARTE_INT_ENDRX;
    nvicEnableIrq(UARTE0_IRQ_NUMBER);

    reg32(UARTE0_BASE, UARTE_ENABLE) = UARTE_ENABLE_ENABLED;
    reg32(UARTE0_BASE, UARTE_TASKS_STARTRX) = 1UL;
    enabled_ = true;
}

// UARTE0 RX interrupt: one byte has landed in g_uarteRxByte via EasyDMA. Copy
// it into the software ring (dropping it only if the ring is full), then re-arm
// RX for the next byte.
extern "C" void UARTE0_UART0_IRQHandler(void) {
    if (reg32(UARTE0_BASE, UARTE_EVENTS_ENDRX) != 0UL) {
        reg32(UARTE0_BASE, UARTE_EVENTS_ENDRX) = 0UL;
        uint16_t next = (g_uarteRxHead + 1) & (UARTE_RX_RING_SIZE - 1);
        if (next != g_uarteRxTail) {
            g_uarteRxRing[g_uarteRxHead] = g_uarteRxByte;
            g_uarteRxHead = next;
        }
        reg32(UARTE0_BASE, UARTE_RXD_PTR) = reinterpret_cast<uint32_t>(&g_uarteRxByte);
        reg32(UARTE0_BASE, UARTE_RXD_MAXCNT) = 1UL;
        reg32(UARTE0_BASE, UARTE_TASKS_STARTRX) = 1UL;
    }
}
