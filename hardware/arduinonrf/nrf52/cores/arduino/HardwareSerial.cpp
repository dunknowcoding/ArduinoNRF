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
constexpr uint32_t UART0_BASE = 0x40002000UL;
constexpr uint32_t NRF_PSEL_DISCONNECTED = 0xFFFFFFFFUL;
constexpr uint32_t UART_TASKS_STARTRX = 0x000UL;
constexpr uint32_t UART_TASKS_STOPRX = 0x004UL;
constexpr uint32_t UART_TASKS_STARTTX = 0x008UL;
constexpr uint32_t UART_TASKS_STOPTX = 0x00CUL;
constexpr uint32_t UART_EVENTS_RXDRDY = 0x108UL;
constexpr uint32_t UART_EVENTS_TXDRDY = 0x11CUL;
constexpr uint32_t UART_ENABLE = 0x500UL;
constexpr uint32_t UART_PSELRTS = 0x508UL;
constexpr uint32_t UART_PSELTXD = 0x50CUL;
constexpr uint32_t UART_PSELCTS = 0x510UL;
constexpr uint32_t UART_PSELRXD = 0x514UL;
constexpr uint32_t UART_RXD = 0x518UL;
constexpr uint32_t UART_TXD = 0x51CUL;
constexpr uint32_t UART_BAUDRATE = 0x524UL;
constexpr uint32_t UART_CONFIG = 0x56CUL;
constexpr uint32_t UART_ENABLE_DISABLED = 0UL;
constexpr uint32_t UART_ENABLE_ENABLED = 4UL;
constexpr uint32_t UART_TIMEOUT_SPINS = 200000UL;

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
    : baudRate_(0), usbBacked_(usbBacked), peekedValue_(-1), enabled_(false) {
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
        reg32(UART0_BASE, UART_TASKS_STOPRX) = 1UL;
        reg32(UART0_BASE, UART_TASKS_STOPTX) = 1UL;
        reg32(UART0_BASE, UART_ENABLE) = UART_ENABLE_DISABLED;
        reg32(UART0_BASE, UART_PSELTXD) = NRF_PSEL_DISCONNECTED;
        reg32(UART0_BASE, UART_PSELRXD) = NRF_PSEL_DISCONNECTED;
        reg32(UART0_BASE, UART_PSELRTS) = NRF_PSEL_DISCONNECTED;
        reg32(UART0_BASE, UART_PSELCTS) = NRF_PSEL_DISCONNECTED;
    } else {
        nrfUsbSerialBackend().end();
    }
    baudRate_ = 0;
    peekedValue_ = -1;
    enabled_ = false;
}

int HardwareSerial::available(void) {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            return nrfUsbdDriver().available();
        }
        return nrfUsbSerialBackend().available();
    }
    pollReceive();
    if (peekedValue_ >= 0) {
        return 1;
    }
    return 0;
}

int HardwareSerial::read(void) {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            return nrfUsbdDriver().read();
        }
        return nrfUsbSerialBackend().read();
    }
    pollReceive();
    int current = peekedValue_;
    peekedValue_ = -1;
    return current;
}

int HardwareSerial::peek(void) {
    if (usbBacked_) {
        if (nrfSerialUsbUsesServicePortOnly()) {
            return nrfUsbdDriver().peek();
        }
        return nrfUsbSerialBackend().peek();
    }
    pollReceive();
    return peekedValue_;
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

    for (uint32_t spin = 0; spin < UART_TIMEOUT_SPINS; ++spin) {
        if (reg32(UART0_BASE, UART_EVENTS_TXDRDY) != 0UL) {
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

    reg32(UART0_BASE, UART_EVENTS_TXDRDY) = 0UL;
    reg32(UART0_BASE, UART_TXD) = value;
    for (uint32_t spin = 0; spin < UART_TIMEOUT_SPINS; ++spin) {
        if (reg32(UART0_BASE, UART_EVENTS_TXDRDY) != 0UL) {
            return 1;
        }
    }
    return 1;
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
    pinMode(PIN_SERIAL_RX, INPUT_PULLUP);
    reg32(UART0_BASE, UART_ENABLE) = UART_ENABLE_DISABLED;
    reg32(UART0_BASE, UART_PSELTXD) = rawTx;
    reg32(UART0_BASE, UART_PSELRXD) = rawRx;
    reg32(UART0_BASE, UART_PSELRTS) = NRF_PSEL_DISCONNECTED;
    reg32(UART0_BASE, UART_PSELCTS) = NRF_PSEL_DISCONNECTED;
    reg32(UART0_BASE, UART_CONFIG) = 0UL;
    unsigned long actualBaudRate = baudRate_;
    if (actualBaudRate == 0UL) {
        actualBaudRate = 115200UL;
    }
    reg32(UART0_BASE, UART_BAUDRATE) = baudRegisterValue(actualBaudRate);
    reg32(UART0_BASE, UART_ENABLE) = UART_ENABLE_ENABLED;
    reg32(UART0_BASE, UART_TASKS_STARTTX) = 1UL;
    reg32(UART0_BASE, UART_TASKS_STARTRX) = 1UL;
    enabled_ = true;
}

void HardwareSerial::pollReceive() {
    if (peekedValue_ >= 0 || usbBacked_ || !enabled_) {
        return;
    }

    if (reg32(UART0_BASE, UART_EVENTS_RXDRDY) != 0UL) {
        reg32(UART0_BASE, UART_EVENTS_RXDRDY) = 0UL;
        peekedValue_ = static_cast<int>(reg32(UART0_BASE, UART_RXD) & 0xFFUL);
    }
}
