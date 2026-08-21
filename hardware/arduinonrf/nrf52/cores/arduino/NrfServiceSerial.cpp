#include "NrfServiceSerial.h"

#include "NrfUsbd.h"
#include "NrfSystem.h"
#include "NrfUsbSerial.h"

NrfServiceSerial SerialService;

void NrfServiceSerial::begin(unsigned long /*baud*/) {
    if (nrfUsbRuntimeEnabled()) {
        nrfUsbdDriver().begin();
    }
}

void NrfServiceSerial::end() {
    // The service CDC cannot be stopped independently of the USB peripheral.
    // Full USB tear-down is managed by the system layer (nrfUsbdDriver().end()).
}

int NrfServiceSerial::available() {
    nrfUsbdDriver().pumpRx(); // fetch any host OUT packet the ISR missed
    return nrfUsbdDriver().available();
}

int NrfServiceSerial::read() {
    nrfUsbdDriver().pumpRx();
    return nrfUsbdDriver().read();
}

int NrfServiceSerial::peek() {
    nrfUsbdDriver().pumpRx();
    return nrfUsbdDriver().peek();
}

void NrfServiceSerial::flush() {
    nrfUsbdDriver().flush();
}

size_t NrfServiceSerial::write(uint8_t value) {
    return nrfUsbdDriver().write(value);
}

int NrfServiceSerial::availableForWrite() {
    if (!nrfUsbdDriver().configured() || nrfUsbdDriver().suspended()) {
        return 0;
    }
    // One slot distinguishes full from empty in the 256-byte ring.
    constexpr size_t capacity = NrfUsbSerialBackend::BufferSize - 1U;
    const size_t pending = nrfUsbdDriver().serviceTxQueued();
    return pending < capacity ? static_cast<int>(capacity - pending) : 0;
}

NrfServiceSerial::operator bool() const {
    return nrfUsbdDriver().configured();
}

bool NrfServiceSerial::connected() const {
    return nrfUsbdDriver().connected();
}

bool NrfServiceSerial::dtr() const {
    return nrfUsbdDriver().dtr();
}

bool NrfServiceSerial::rts() const {
    return nrfUsbdDriver().rts();
}

unsigned long NrfServiceSerial::baud() const {
    return nrfUsbdDriver().baud();
}
