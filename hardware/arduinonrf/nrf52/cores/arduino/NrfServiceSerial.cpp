#include "NrfServiceSerial.h"

#include "NrfUsbd.h"
#include "NrfSystem.h"

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
    return nrfUsbdDriver().available();
}

int NrfServiceSerial::read() {
    return nrfUsbdDriver().read();
}

int NrfServiceSerial::peek() {
    return nrfUsbdDriver().peek();
}

void NrfServiceSerial::flush() {
    nrfUsbdDriver().flush();
}

size_t NrfServiceSerial::write(uint8_t value) {
    return nrfUsbdDriver().write(value);
}

int NrfServiceSerial::availableForWrite() {
    // The driver's service CDC ring buffer holds 256 bytes (NrfUsbdDriver::
    // RingBufferSize).  Return a non-zero value whenever the USB device is
    // configured so that Print::write() does not drop bytes.
    return nrfUsbdDriver().configured() ? 256 : 0;
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
