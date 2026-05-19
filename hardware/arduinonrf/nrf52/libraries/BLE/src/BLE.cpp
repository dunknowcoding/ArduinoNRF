#include "BLE.h"

#include <string.h>

#include <NrfBleHw.h>
#include <NrfClock.h>
#include <NrfSystem.h>

namespace {
NrfBleRadio g_radio;
uint8_t g_advPayload[31] = {0};

// The current BLE facade is intentionally gated by the declared low-frequency
// clock source because the repository does not ship a board-agnostic SoftDevice
// or Bluefruit stack. If the clock source is still unknown, the facade reports
// BLE as unavailable instead of pretending that advertising is safe to start.
bool bleClockSourceDeclared() {
    return nrfClockProfile().lowFrequencyClockDeclared;
}

int hexNibble(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

size_t copyServiceField(const char *uuid, uint8_t *payload, size_t offset, size_t capacity) {
    if (uuid == nullptr || uuid[0] == '\0' || offset + 4 > capacity) {
        return offset;
    }

    const size_t uuidLength = strlen(uuid);
    if (uuidLength == 4) {
        const int n0 = hexNibble(uuid[0]);
        const int n1 = hexNibble(uuid[1]);
        const int n2 = hexNibble(uuid[2]);
        const int n3 = hexNibble(uuid[3]);
        if (n0 < 0 || n1 < 0 || n2 < 0 || n3 < 0 || offset + 4 > capacity) {
            return offset;
        }

        payload[offset++] = 3;
        payload[offset++] = 0x03;
        payload[offset++] = static_cast<uint8_t>((n2 << 4) | n3);
        payload[offset++] = static_cast<uint8_t>((n0 << 4) | n1);
        return offset;
    }

    uint8_t uuidBytes[16] = {0};
    size_t nibbleCount = 0;
    for (size_t index = 0; index < uuidLength; ++index) {
        if (uuid[index] == '-') {
            continue;
        }
        const int nibble = hexNibble(uuid[index]);
        if (nibble < 0 || nibbleCount >= 32) {
            return offset;
        }
        if ((nibbleCount & 1U) == 0U) {
            uuidBytes[nibbleCount / 2] = static_cast<uint8_t>(nibble << 4);
        } else {
            uuidBytes[nibbleCount / 2] = static_cast<uint8_t>(uuidBytes[nibbleCount / 2] | nibble);
        }
        ++nibbleCount;
    }

    if (nibbleCount != 32 || offset + 18 > capacity) {
        return offset;
    }

    payload[offset++] = 17;
    payload[offset++] = 0x07;
    for (int index = 15; index >= 0; --index) {
        payload[offset++] = uuidBytes[index];
    }
    return offset;
}

// Build a compact advertising field directly into the static payload buffer.
// The implementation keeps the payload bounded to legacy advertising size and
// truncates long strings rather than overrunning the buffer.
size_t copyTextField(uint8_t fieldType, const char *text, uint8_t *payload, size_t offset, size_t capacity) {
    if (text == nullptr || text[0] == '\0' || offset + 2 >= capacity) {
        return offset;
    }

    size_t textLength = strlen(text);
    if (textLength > 26) {
        textLength = 26;
    }
    if (offset + textLength + 2 > capacity) {
        textLength = capacity - offset - 2;
    }
    payload[offset++] = static_cast<uint8_t>(textLength + 1);
    payload[offset++] = fieldType;
    memcpy(payload + offset, text, textLength);
    return offset + textLength;
}
}

BLEClass BLE;

bool BLECharacteristic::writeValue(const uint8_t *data, size_t length) {
    if (data == nullptr) {
        return false;
    }
    if (length > sizeof(value_)) {
        length = sizeof(value_);
    }
    memcpy(value_, data, length);
    valueLength_ = length;
    return true;
}

bool BLEService::addCharacteristic(BLECharacteristic &characteristic) {
    if (characteristicCount_ >= (sizeof(characteristics_) / sizeof(characteristics_[0]))) {
        return false;
    }
    characteristics_[characteristicCount_++] = &characteristic;
    return true;
}

bool BLEClass::begin() {
    // This package only exposes the minimal self-hosted advertising facade.
    // Starting the LFCLK/HFCLK here matches the expectations of the radio shim
    // while keeping the public API consistent with Arduino BLE-style sketches.
    if (!radioSupported()) {
        return false;
    }
    nrfStartLfclk();
    nrfStartHfclk();
    g_radio.begin();
    setTxPower(0);
    return true;
}

bool BLEClass::begin(const BLEAdvConfig &config) {
    if (!begin()) {
        return false;
    }

    setDeviceName(config.deviceName);
    setLocalName(config.localName);
    setTxPower(config.txPower);
    setAdvertisingInterval(config.interval);
    return true;
}

void BLEClass::end() {
    g_radio.end();
}

bool BLEClass::setAdvertisedServiceUuid(const char *uuid) {
    if (uuid == nullptr) {
        advertisedServiceUuid_[0] = '\0';
        return false;
    }
    strncpy(advertisedServiceUuid_, uuid, sizeof(advertisedServiceUuid_) - 1);
    advertisedServiceUuid_[sizeof(advertisedServiceUuid_) - 1] = '\0';
    return advertisedServiceUuid_[0] != '\0';
}

bool BLEClass::setAdvertisedService(const BLEService &service) {
    return setAdvertisedServiceUuid(service.uuid());
}

void BLEClass::setDeviceName(const char *name) {
    if (name == nullptr) {
        deviceName_[0] = '\0';
        return;
    }
    strncpy(deviceName_, name, sizeof(deviceName_) - 1);
    deviceName_[sizeof(deviceName_) - 1] = '\0';
}

void BLEClass::setLocalName(const char *name) {
    if (name == nullptr) {
        localName_[0] = '\0';
        return;
    }
    strncpy(localName_, name, sizeof(localName_) - 1);
    localName_[sizeof(localName_) - 1] = '\0';
}

void BLEClass::setTxPower(int8_t powerDbm) {
    g_radio.setTxPower(powerDbm);
}

void BLEClass::setAdvertisingInterval(uint16_t intervalUnits) {
    g_radio.setAdvertisingIntervalUnits(intervalUnits);
}

void BLEClass::setManufacturerData(const uint8_t *data, size_t length) {
    g_radio.setAdvertisingData(data, length);
}

bool BLEClass::addService(BLEService &service) {
    if (!serviceRegistrySupported()) {
        return false;
    }
    if (serviceCount_ >= (sizeof(services_) / sizeof(services_[0]))) {
        return false;
    }
    services_[serviceCount_++] = &service;
    return true;
}

bool BLEClass::advertise() {
    if (!advertisingSupported()) {
        return false;
    }

    // Prefer the explicit local name, then fall back to the device name so the
    // payload remains deterministic even when sketches only configure one of the
    // two common Arduino naming entry points.
    size_t offset = 0;
    memset(g_advPayload, 0, sizeof(g_advPayload));
    offset = copyServiceField(advertisedServiceUuid_, g_advPayload, offset, sizeof(g_advPayload));
    offset = copyTextField(0x09, localName_, g_advPayload, offset, sizeof(g_advPayload));
    if (offset == 0) {
        offset = copyTextField(0x09, deviceName_, g_advPayload, offset, sizeof(g_advPayload));
    }
    g_radio.setAdvertisingData(g_advPayload, offset);
    g_radio.startAdvertising();
    return true;
}

bool BLEClass::adv(const BLEAdvConfig &config) {
    if (!begin(config)) {
        return false;
    }
    return advertise();
}

void BLEClass::stopAdvertise() {
    g_radio.stopAdvertising();
}

bool BLEClass::advertising() const {
    return g_radio.isAdvertising();
}

bool BLEClass::supported() const {
    return bleClockSourceDeclared();
}

bool BLEClass::radioSupported() const {
    return bleClockSourceDeclared();
}

bool BLEClass::advertisingSupported() const {
    return bleClockSourceDeclared();
}

bool BLEClass::serviceRegistrySupported() const {
    return bleClockSourceDeclared();
}

bool BLEClass::gattServerSupported() const {
    return false;
}

bool BLEClass::connectionsSupported() const {
    return false;
}

bool BLEClass::notificationsSupported() const {
    return false;
}

bool BLEClass::subscriptionStateSupported() const {
    return false;
}

bool BLEClass::otaDfuSupported() const {
    return false;
}

bool BLEClass::selfHostedStack() const {
    return true;
}

bool BLEClass::bluefruitBacked() const {
    return false;
}

bool BLEClass::facadeOnly() const {
    // Report the facade boundary explicitly so docs, smoke tests, and user code
    // can distinguish "advertising only" from a full GATT-capable BLE stack.
    return selfHostedStack() && (!gattServerSupported() || !connectionsSupported() || !notificationsSupported() || !subscriptionStateSupported() || !otaDfuSupported());
}

bool BLEClass::clockSourceDeclared() const {
    return bleClockSourceDeclared();
}

const char *BLEClass::lowFrequencyClockSource() const {
    return nrfClockProfile().lowFrequencyClockSource;
}

const char *BLEClass::clockSourceEvidenceLevel() const {
    return nrfClockProfile().clockSourceEvidenceLevel;
}

const char *BLEClass::implementationName() const {
    return "arduinonrf-minimal-ble";
}

const char *BLEClass::statusMessage() const {
    // Keep status strings blunt and capability-oriented because the repository
    // currently documents BLE as a constrained facade rather than a full stack.
    if (!clockSourceDeclared()) {
        return "BLE facade disabled: low-frequency clock source undeclared";
    }
    if (advertising()) {
        if (facadeOnly()) {
            return "advertising-only self-hosted BLE active";
        }
        return "BLE active";
    }
    if (facadeOnly()) {
        return "advertising-only self-hosted BLE ready";
    }
    return "BLE idle";
}

const char *BLEClass::deviceName() const {
    return deviceName_;
}

void BLEClass::poll() {
}
