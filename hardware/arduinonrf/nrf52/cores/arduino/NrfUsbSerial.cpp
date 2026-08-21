#include "NrfUsbSerial.h"

#include "Arduino.h"
#include "NrfSystem.h"
#include "NrfUsbd.h"

NrfUsbSerialBackend &nrfUsbSerialBackend() {
    static NrfUsbSerialBackend backend;
    return backend;
}

void NrfUsbSerialBackend::begin(unsigned long baudRate) {
    (void)baudRate;
    if (nrfUsbRuntimeEnabled()) {
        nrfUsbdDriver().begin();
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
