#include "NrfUsbSerial.h"

#include "Arduino.h"
#include "NrfSystem.h"
#include "NrfUsbd.h"

NrfUsbSerialBackend &nrfUsbSerialBackend() {
    static NrfUsbSerialBackend backend;
    return backend;
}

unsigned long NrfUsbSerialBackend::normalizedBaud(unsigned long baudRate) {
    return baudRate == 0UL ? 115200UL : baudRate;
}

void NrfUsbSerialBackend::begin(unsigned long baudRate) {
    baudRate = normalizedBaud(baudRate);
    if (nrfUsbRuntimeEnabled()) {
        nrfUsbdDriver().begin();
    }
    if (nrfUsbUserPortEnabled()) {
        NrfUsbLineCoding lineCoding = nrfUsbdDriver().userLineCoding();
        lineCoding.baudRate = static_cast<uint32_t>(baudRate);
        nrfUsbdDriver().setUserLineCoding(lineCoding);
    }
}

void NrfUsbSerialBackend::end() {
}

int NrfUsbSerialBackend::available() const {
    nrfUsbdDriver().pumpRx(); // fetch any host OUT packet the ISR missed
    return nrfUsbdDriver().userAvailable();
}

int NrfUsbSerialBackend::read() {
    nrfUsbdDriver().pumpRx();
    return nrfUsbdDriver().userRead();
}

int NrfUsbSerialBackend::peek() const {
    nrfUsbdDriver().pumpRx();
    return nrfUsbdDriver().userPeek();
}

void NrfUsbSerialBackend::flush() {
    nrfUsbdDriver().userFlush();
}

size_t NrfUsbSerialBackend::write(uint8_t value) {
    return nrfUsbdDriver().userWrite(value);
}

size_t NrfUsbSerialBackend::write(const uint8_t *data, size_t length) {
    return nrfUsbdDriver().userWrite(data, length);
}

bool NrfUsbSerialBackend::connected() const {
    return nrfUsbdDriver().userConnected();
}

void NrfUsbSerialBackend::setLineCoding(unsigned long baudRate) {
    baudRate = normalizedBaud(baudRate);
    NrfUsbLineCoding lineCoding = nrfUsbdDriver().userLineCoding();
    lineCoding.baudRate = static_cast<uint32_t>(baudRate);
    nrfUsbdDriver().setUserLineCoding(lineCoding);
}

void NrfUsbSerialBackend::setLineState(bool dtr, bool rts) {
    nrfUsbdDriver().setUserLineState(dtr, rts);
}

bool NrfUsbSerialBackend::dtr() const {
    return nrfUsbdDriver().userDtr();
}

bool NrfUsbSerialBackend::rts() const {
    return nrfUsbdDriver().userRts();
}

unsigned long NrfUsbSerialBackend::baud() const {
    return nrfUsbdDriver().userBaud();
}

size_t NrfUsbSerialBackend::txPending() const {
    return nrfUsbdDriver().userTxQueued();
}

void NrfUsbSerialBackend::injectRx(const uint8_t *data, size_t length) {
    nrfUsbdDriver().injectRx(data, length);
}

void NrfUsbSerialBackend::poll() {
    nrfUsbdDriver().poll();
}

void NrfUsbSerialBackend::configureFromSystemProfile() {
    if (nrfUsbRuntimeEnabled()) {
        nrfUsbdDriver().begin();
    }
}

bool NrfUsbSerialBackend::usbPeripheralEnabled() const {
    return nrfUsbdDriver().enabled();
}
