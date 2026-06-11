#include "NrfUsbSerial.h"

#include "Arduino.h"
#include "NrfSystem.h"
#include "NrfUsbd.h"

NrfUsbSerialBackend &nrfUsbSerialBackend() {
    static NrfUsbSerialBackend backend;
    return backend;
}

void NrfUsbSerialBackend::begin(unsigned long baudRate) {
    baudRate_ = baudRate;
    if (baudRate_ == 0UL) {
        baudRate_ = 115200UL;
    }
    if (nrfUsbRuntimeEnabled()) {
        nrfUsbdDriver().begin();
    }
    if (nrfUsbUserPortEnabled()) {
        NrfUsbLineCoding lineCoding = nrfUsbdDriver().userLineCoding();
        lineCoding.baudRate = static_cast<uint32_t>(baudRate_);
        nrfUsbdDriver().setUserLineCoding(lineCoding);
        connected_ = nrfUsbdDriver().userConnected();
    } else {
        connected_ = false;
    }
}

void NrfUsbSerialBackend::end() {
    connected_ = false;
    dtr_ = false;
    rts_ = false;
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

bool NrfUsbSerialBackend::connected() const {
    return nrfUsbdDriver().userConnected();
}

void NrfUsbSerialBackend::setConnected(bool connected) {
    connected_ = connected;
}

void NrfUsbSerialBackend::setLineCoding(unsigned long baudRate) {
    baudRate_ = baudRate;
    if (baudRate_ == 0UL) {
        baudRate_ = 115200UL;
    }
    NrfUsbLineCoding lineCoding = nrfUsbdDriver().userLineCoding();
    lineCoding.baudRate = static_cast<uint32_t>(baudRate_);
    nrfUsbdDriver().setUserLineCoding(lineCoding);
}

void NrfUsbSerialBackend::setLineState(bool dtr, bool rts) {
    dtr_ = dtr;
    rts_ = rts;
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
    return 0U;
}

void NrfUsbSerialBackend::injectRx(const uint8_t *data, size_t length) {
    nrfUsbdDriver().injectRx(data, length);
}

void NrfUsbSerialBackend::poll() {
    nrfUsbdDriver().poll();
    baudRate_ = nrfUsbdDriver().userBaud();
    dtr_ = nrfUsbdDriver().userDtr();
    rts_ = nrfUsbdDriver().userRts();
    connected_ = nrfUsbdDriver().userConnected();
}

void NrfUsbSerialBackend::configureFromSystemProfile() {
    if (nrfUsbRuntimeEnabled()) {
#if defined(NRF_BOARD_HAS_NATIVE_USB) && (NRF_BOARD_HAS_NATIVE_USB == 1)
        // After Adafruit-style serial DFU the host tears down the bootloader
        // composite device and the on-chip USB regulator/front-end can still be
        // settling when the app starts. Beginning USBD in the same millisecond
        // window has been observed on ProMicro-class clones to yield no new COM
        // (clean disconnect in Device Manager, never re-enumerates).
        delay(400);
#endif
        nrfUsbdDriver().begin();
    }
    if (nrfUsbUserPortEnabled()) {
        connected_ = nrfUsbdDriver().userConnected();
    } else {
        connected_ = false;
    }
}

bool NrfUsbSerialBackend::usbPeripheralEnabled() const {
    return nrfUsbdDriver().enabled();
}

void NrfUsbSerialBackend::pushRx(uint8_t value) {
    (void)value;
}

bool NrfUsbSerialBackend::pushTx(uint8_t value) {
    return nrfUsbdDriver().userWrite(value) == 1U;
}

int NrfUsbSerialBackend::popRx() {
    return nrfUsbdDriver().userRead();
}
