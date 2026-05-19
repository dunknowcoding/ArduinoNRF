#include "USBDevice.h"

#include "NrfBoard.h"
#include "NrfSystem.h"
#include "NrfUsbd.h"
#include "NrfUsbSerial.h"

USBDeviceClass USBDevice;

void USBDeviceClass::init() {
    if (!nrfUsbRuntimeEnabled()) {
        return;
    }
    nrfUsbdDriver().begin();
}

void USBDeviceClass::attach() {
    if (!nrfUsbRuntimeEnabled()) {
        return;
    }
    nrfUsbdDriver().attach();
}

void USBDeviceClass::detach() {
    if (!nrfUsbRuntimeEnabled()) {
        return;
    }
    nrfUsbdDriver().detach();
    nrfUsbSerialBackend().setConnected(false);
}

void USBDeviceClass::poll() {
    if (!initialized()) {
        return;
    }
    nrfUsbdDriver().poll();
}

bool USBDeviceClass::initialized() const {
    return nrfUsbRuntimeEnabled() && nrfUsbdDriver().enabled();
}

bool USBDeviceClass::attached() const {
    return initialized() && nrfUsbdDriver().attached();
}

bool USBDeviceClass::configured() const {
    return attached() && nrfUsbdDriver().configured();
}

bool USBDeviceClass::connected() const {
    return attached() && nrfUsbdDriver().connected();
}

bool USBDeviceClass::ready() const {
    return attached() && nrfUsbdDriver().ready();
}

bool USBDeviceClass::suspended() const {
    return attached() && nrfUsbdDriver().suspended();
}

bool USBDeviceClass::wakeupHost() {
    return false;
}

uint8_t USBDeviceClass::address() const {
    if (initialized()) {
        return nrfUsbdDriver().address();
    }
    return 0U;
}

uint8_t USBDeviceClass::configuration() const {
    if (initialized()) {
        return nrfUsbdDriver().configuration();
    }
    return 0U;
}

NrfUsbdStatus USBDeviceClass::status() const {
    if (!initialized()) {
        return {false, false, false, false, false, false, false, false, 0U, 0U, 0UL};
    }
    return nrfUsbdDriver().status();
}

USBDeviceClass::operator bool() const {
    return connected();
}
