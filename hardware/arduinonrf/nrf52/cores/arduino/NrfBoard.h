#pragma once

#include <stdint.h>
#include <variant.h>

struct NrfBoardInfo {
    const char *name;
    const char *family;
    const char *batteryModelEvidenceLevel;
    const char *uploadProfileEvidenceLevel;
    const char *clockSourceEvidenceLevel;
    const char *lowFrequencyClockSource;
    uint8_t pinCount;
    bool hasNativeUsb;
    bool hasBatterySense;
    bool hasQspiFlash;
    bool hasRgbLed;
    bool hasWiFiCoprocessor;
    bool hasImu;
    bool hasNfc;
    bool batterySenseViaVddhDiv5;
    bool pinMapVerified;
    bool batteryModelVerified;
    bool uploadProfileVerified;
    bool lowPowerProfileModeled;
};

struct NrfBoardSupportStatus {
    const char *family;
    const char *batteryModelEvidenceLevel;
    const char *uploadProfileEvidenceLevel;
    const char *clockSourceEvidenceLevel;
    const char *lowFrequencyClockSource;
    bool pinMapVerified;
    bool batteryModelVerified;
    bool uploadProfileVerified;
    bool lowPowerProfileModeled;
};

struct NrfBoardPowerInfo {
    bool batterySenseAvailable;
    bool batterySenseViaVddhDiv5;
    bool extVccControlAvailable;
    bool chargeStatusAvailable;
    bool usbPowerPresent;
    bool usbBatteryCoexistencePossible;
    bool usbBatteryCoexistenceActive;
    uint8_t batterySensePin;
    uint8_t extVccControlPin;
    uint8_t chargeStatusPin;
    uint8_t batteryVoltageScaleNumerator;
    uint8_t batteryVoltageScaleDenominator;
};

struct NrfBoardBusInfo {
    bool secondarySpiAvailable;
    bool secondaryWireAvailable;
    bool spiDefaultPinsVerified;
    bool serial1PinsVerified;
    bool secondarySpiPinsVerified;
    bool secondaryWirePinsVerified;
    uint8_t secondarySpiMisoPin;
    uint8_t secondarySpiMosiPin;
    uint8_t secondarySpiSckPin;
    uint8_t secondaryWireSdaPin;
    uint8_t secondaryWireSclPin;
};

struct NrfPinInfo {
    bool valid;
    bool rawMappingKnown;
    bool analogInput;
    bool pwmOutput;
    bool powerSense;
    bool powerControl;
    bool chargeStatus;
    bool serialRx;
    bool serialTx;
    bool wireSda;
    bool wireScl;
    bool spiMiso;
    bool spiMosi;
    bool spiSck;
    bool qspi;
    uint8_t rawPort;
    uint8_t rawIndex;
    uint8_t rawPin;
};

const NrfBoardCapabilities &nrfBoardCapabilities();
NrfBoardInfo nrfBoardInfo();
NrfBoardSupportStatus nrfBoardSupportStatus();
NrfBoardPowerInfo nrfBoardPowerInfo();
NrfBoardBusInfo nrfBoardBusInfo();
NrfPinInfo nrfPinInfo(uint8_t pin);
const char *nrfBoardName();
const char *nrfBoardFamily();
const char *nrfBoardBatteryModelEvidenceLevel();
const char *nrfBoardUploadProfileEvidenceLevel();
const char *nrfBoardClockSourceEvidenceLevel();
const char *nrfBoardLowFrequencyClockSource();
bool nrfBoardHasNativeUsb();
bool nrfBoardHasBatterySense();
bool nrfBoardHasQspiFlash();
bool nrfBoardHasRgbLed();
bool nrfBoardHasWiFiCoprocessor();
bool nrfBoardHasImu();
bool nrfBoardHasNfc();
bool nrfBoardHasUsbCdc();
bool nrfBoardHasSwdDebug();
bool nrfBoardPinMapVerified();
bool nrfBoardBatteryModelVerified();
bool nrfBoardUploadProfileVerified();
bool nrfBoardLowPowerProfileModeled();
bool nrfBoardBatterySenseViaVddhDiv5();
uint8_t nrfBoardBatteryVoltageScaleNumerator();
uint8_t nrfBoardBatteryVoltageScaleDenominator();
bool nrfBoardUsbPowerPresent();
bool nrfBoardUsbBatteryCoexistencePossible();
bool nrfBoardUsbBatteryCoexistenceActive();
bool nrfBoardHasSecondarySpi();
bool nrfBoardHasSecondaryWire();
bool nrfBoardSpiDefaultPinsVerified();
bool nrfBoardSerial1PinsVerified();
bool nrfBoardSpi1PinsVerified();
bool nrfBoardWire1PinsVerified();
bool nrfBoardHasExtVccControl();
bool nrfBoardHasChargeStatus();
bool nrfBoardPinValid(uint8_t pin);
bool nrfBoardPinSupportsAnalogInput(uint8_t pin);
bool nrfBoardPinSupportsPwm(uint8_t pin);
