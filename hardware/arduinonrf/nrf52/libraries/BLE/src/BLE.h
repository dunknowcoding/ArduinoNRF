#pragma once

#include <string.h>
#include <stddef.h>
#include <stdint.h>

class BLECharacteristic {
public:
    BLECharacteristic(const char *uuid, uint8_t properties, size_t valueSize = 20)
        : uuid_(uuid), properties_(properties), valueLength_(valueSize) {
        if (valueLength_ > sizeof(value_)) {
            valueLength_ = sizeof(value_);
        }
        memset(value_, 0, sizeof(value_));
    }

    const char *uuid() const { return uuid_; }
    uint8_t properties() const { return properties_; }
    size_t valueLength() const { return valueLength_; }
    bool writeValue(const uint8_t *data, size_t length);
    const uint8_t *value() const { return value_; }

private:
    const char *uuid_;
    uint8_t properties_;
    size_t valueLength_;
    uint8_t value_[20];
};

class BLEService {
public:
    explicit BLEService(const char *uuid) : uuid_(uuid) {}

    const char *uuid() const { return uuid_; }
    bool addCharacteristic(BLECharacteristic &characteristic);
    size_t characteristicCount() const { return characteristicCount_; }

private:
    const char *uuid_;
    BLECharacteristic *characteristics_[8] = {nullptr};
    size_t characteristicCount_ = 0;
};

struct BLEAdvConfig {
    const char *deviceName;
    const char *localName;
    int8_t txPower;
    uint16_t interval;
};

class BLEClass {
public:
    bool begin();
    bool begin(const BLEAdvConfig &config);
    void end();
    bool setAdvertisedServiceUuid(const char *uuid);
    bool setAdvertisedService(const BLEService &service);
    void setDeviceName(const char *name);
    void setLocalName(const char *name);
    void setTxPower(int8_t powerDbm);
    void setAdvertisingInterval(uint16_t intervalUnits);
    void setManufacturerData(const uint8_t *data, size_t length);
    bool addService(BLEService &service);
    bool advertise();
    bool adv(const BLEAdvConfig &config);
    bool adv() { return advertise(); }
    void stopAdvertise();
    bool advertising() const;
    bool startAdvertising() { return advertise(); }
    bool isAdvertising() const { return advertising(); }
    bool supported() const;
    bool radioSupported() const;
    bool advertisingSupported() const;
    bool serviceRegistrySupported() const;
    bool hasServiceRegistry() const { return serviceRegistrySupported(); }
    bool gattServerSupported() const;
    bool hasGattServer() const { return gattServerSupported(); }
    bool connectionsSupported() const;
    bool hasConnections() const { return connectionsSupported(); }
    bool notificationsSupported() const;
    bool canNotify() const { return notificationsSupported(); }
    bool subscriptionStateSupported() const;
    bool keepsSubscriptions() const { return subscriptionStateSupported(); }
    bool otaDfuSupported() const;
    bool hasOtaDfu() const { return otaDfuSupported(); }
    bool selfHostedStack() const;
    bool bluefruitBacked() const;
    bool usingBluefruit() const { return bluefruitBacked(); }
    bool facadeOnly() const;
    bool usesMinimalStack() const { return facadeOnly(); }
    bool clockSourceDeclared() const;
    bool hasLowFrequencyClock() const { return clockSourceDeclared(); }
    const char *lowFrequencyClockSource() const;
    const char *clockSourceEvidenceLevel() const;
    const char *implementationName() const;
    const char *statusMessage() const;
    const char *deviceName() const;
    void poll();

private:
    char deviceName_[20] = "ArduinoNRF";
    char localName_[20] = "ArduinoNRF";
    char advertisedServiceUuid_[37] = {0};
    BLEService *services_[8] = {nullptr};
    size_t serviceCount_ = 0;
};

extern BLEClass BLE;
