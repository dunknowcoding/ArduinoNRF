#include "NrfBoard.h"

#include "Arduino.h"
#include "NrfSystem.h"
#include "NrfUsbd.h"

namespace {
// Variants use 0xFF as the sentinel for "not assigned to any public Arduino pin".
// Centralizing that reachability check keeps the board-truth helpers consistent
// across power pins, QSPI pins, and optional secondary buses.
bool boardPinReachable(uint8_t pin) {
    return pin != 0xFFU && pin < PINS_COUNT;
}

bool boardFamilyEquals(const char *family) {
    const char *current = nrfBoardCapabilities().boardFamily;
    if (current == nullptr || family == nullptr) {
        return false;
    }
    return strcmp(current, family) == 0;
}

bool rawPinReachable(uint32_t rawPin) {
    return rawPin != 0xFFFFFFFFUL && rawPin != 0xFFUL && rawPin < 64UL;
}

uint32_t rawPinForBoardPin(uint8_t pin) {
    if (!boardPinReachable(pin)) {
        return 0xFFFFFFFFUL;
    }
    return g_ADigitalPinMap[pin];
}

bool pinIsQspi(uint8_t pin) {
    return pin == PIN_QSPI_CS || pin == PIN_QSPI_SCK || pin == PIN_QSPI_IO0 || pin == PIN_QSPI_IO1 || pin == PIN_QSPI_IO2 || pin == PIN_QSPI_IO3;
}

bool qspiPinsReachable() {
    return boardPinReachable(PIN_QSPI_CS) &&
        boardPinReachable(PIN_QSPI_SCK) &&
        boardPinReachable(PIN_QSPI_IO0) &&
        boardPinReachable(PIN_QSPI_IO1) &&
        boardPinReachable(PIN_QSPI_IO2) &&
        boardPinReachable(PIN_QSPI_IO3);
}

bool rgbPinsReachable() {
    return boardPinReachable(PIN_LED1) && boardPinReachable(PIN_LED2) && boardPinReachable(PIN_LED3);
}

bool lowFrequencyClockDeclared() {
    // BLE and low-power helpers are intentionally gated by declared board
    // metadata so downstream code can distinguish documented clock facts from
    // boards that are still only partially modeled.
    const char *clockSource = nrfBoardCapabilities().lowFrequencyClockSource;
    return clockSource != nullptr && strcmp(clockSource, "undeclared") != 0;
}
}

const NrfBoardCapabilities &nrfBoardCapabilities() {
    return g_boardCapabilities;
}

NrfBoardInfo nrfBoardInfo() {
    const NrfBoardCapabilities &capabilities = nrfBoardCapabilities();
    return {
        capabilities.boardName,
        capabilities.boardFamily,
        capabilities.batteryModelEvidenceLevel,
        capabilities.uploadProfileEvidenceLevel,
        capabilities.clockSourceEvidenceLevel,
        capabilities.lowFrequencyClockSource,
        PINS_COUNT,
        capabilities.hasNativeUsb,
        capabilities.hasBatterySense && boardPinReachable(PIN_BATTERY),
        capabilities.hasQspiFlash && qspiPinsReachable(),
        capabilities.hasRgbLed && rgbPinsReachable(),
        capabilities.hasWiFiCoprocessor,
        capabilities.hasImu,
        capabilities.hasNfc,
        capabilities.batterySenseViaVddhDiv5,
        capabilities.pinMapVerified,
        capabilities.batteryModelVerified,
        capabilities.uploadProfileVerified,
        capabilities.lowPowerProfileModeled && lowFrequencyClockDeclared(),
    };
}

NrfBoardSupportStatus nrfBoardSupportStatus() {
    const NrfBoardCapabilities &capabilities = nrfBoardCapabilities();
    return {
        capabilities.boardFamily,
        capabilities.batteryModelEvidenceLevel,
        capabilities.uploadProfileEvidenceLevel,
        capabilities.clockSourceEvidenceLevel,
        capabilities.lowFrequencyClockSource,
        capabilities.pinMapVerified,
        capabilities.batteryModelVerified,
        capabilities.uploadProfileVerified,
        capabilities.lowPowerProfileModeled && lowFrequencyClockDeclared(),
    };
}

NrfBoardPowerInfo nrfBoardPowerInfo() {
    // The power model reports only the pins and scaling facts that are both
    // declared by the active variant and reachable in the current pin map.
    const bool batterySenseAvailable = nrfBoardHasBatterySense();
    const bool usbPowerPresent = nrfBoardUsbPowerPresent();
    uint8_t batterySensePin = static_cast<uint8_t>(0xFFU);
    if (batterySenseAvailable && boardPinReachable(PIN_BATTERY)) {
        batterySensePin = PIN_BATTERY;
    }
    uint8_t extVccControlPin = static_cast<uint8_t>(0xFFU);
    if (boardPinReachable(PIN_EXT_VCC)) {
        extVccControlPin = PIN_EXT_VCC;
    }
    uint8_t chargeStatusPin = static_cast<uint8_t>(0xFFU);
    if (boardPinReachable(PIN_CHARGE_STATUS)) {
        chargeStatusPin = PIN_CHARGE_STATUS;
    }
    return {
        batterySenseAvailable,
        nrfBoardBatterySenseViaVddhDiv5(),
        boardPinReachable(PIN_EXT_VCC),
        boardPinReachable(PIN_CHARGE_STATUS),
        usbPowerPresent,
        nrfBoardUsbBatteryCoexistencePossible(),
        batterySenseAvailable && usbPowerPresent,
        batterySensePin,
        extVccControlPin,
        chargeStatusPin,
        nrfBoardBatteryVoltageScaleNumerator(),
        nrfBoardBatteryVoltageScaleDenominator(),
    };
}

NrfBoardBusInfo nrfBoardBusInfo() {
    // Secondary bus availability is a metadata truth surface for optional pins.
    // It does not imply that a second global Wire1 or SPI1 object already
    // exists in the public Arduino API of this package.
    uint8_t spi1Miso = static_cast<uint8_t>(0xFFU);
    if (boardPinReachable(PIN_SPI1_MISO)) {
        spi1Miso = PIN_SPI1_MISO;
    }
    uint8_t spi1Mosi = static_cast<uint8_t>(0xFFU);
    if (boardPinReachable(PIN_SPI1_MOSI)) {
        spi1Mosi = PIN_SPI1_MOSI;
    }
    uint8_t spi1Sck = static_cast<uint8_t>(0xFFU);
    if (boardPinReachable(PIN_SPI1_SCK)) {
        spi1Sck = PIN_SPI1_SCK;
    }
    uint8_t wire1Sda = static_cast<uint8_t>(0xFFU);
    if (boardPinReachable(PIN_WIRE1_SDA)) {
        wire1Sda = PIN_WIRE1_SDA;
    }
    uint8_t wire1Scl = static_cast<uint8_t>(0xFFU);
    if (boardPinReachable(PIN_WIRE1_SCL)) {
        wire1Scl = PIN_WIRE1_SCL;
    }
    return {
        spi1Miso != 0xFFU && spi1Mosi != 0xFFU && spi1Sck != 0xFFU,
        wire1Sda != 0xFFU && wire1Scl != 0xFFU,
        nrfBoardSpiDefaultPinsVerified(),
        nrfBoardSerial1PinsVerified(),
        nrfBoardSpi1PinsVerified(),
        nrfBoardWire1PinsVerified(),
        spi1Miso,
        spi1Mosi,
        spi1Sck,
        wire1Sda,
        wire1Scl,
    };
}

NrfPinInfo nrfPinInfo(uint8_t pin) {
    const bool valid = boardPinReachable(pin);
    const uint32_t rawPin = rawPinForBoardPin(pin);
    const bool rawMappingKnown = rawPinReachable(rawPin);
    const bool powerSense = valid && pin == PIN_BATTERY;
    const bool powerControl = valid && pin == PIN_EXT_VCC;
    const bool chargeStatus = valid && pin == PIN_CHARGE_STATUS;
    const bool pwmOutput = valid && !powerSense && !powerControl && !chargeStatus;
    uint8_t rawPort = static_cast<uint8_t>(0xFFU);
    uint8_t rawIndex = static_cast<uint8_t>(0xFFU);
    uint8_t rawPinByte = static_cast<uint8_t>(0xFFU);
    if (rawMappingKnown) {
        rawPort = static_cast<uint8_t>(rawPin >> 5U);
        rawIndex = static_cast<uint8_t>(rawPin & 0x1FU);
        rawPinByte = static_cast<uint8_t>(rawPin);
    }
    return {
        valid,
        rawMappingKnown,
        valid && nrfAnalogInputSupported(pin),
        pwmOutput,
        powerSense,
        powerControl,
        chargeStatus,
        valid && pin == PIN_SERIAL_RX,
        valid && pin == PIN_SERIAL_TX,
        valid && pin == PIN_WIRE_SDA,
        valid && pin == PIN_WIRE_SCL,
        valid && pin == PIN_SPI_MISO,
        valid && pin == PIN_SPI_MOSI,
        valid && pin == PIN_SPI_SCK,
        valid && pinIsQspi(pin),
        rawPort,
        rawIndex,
        rawPinByte,
    };
}

const char *nrfBoardName() {
    return nrfBoardCapabilities().boardName;
}

const char *nrfBoardFamily() {
    return nrfBoardCapabilities().boardFamily;
}

const char *nrfBoardBatteryModelEvidenceLevel() {
    return nrfBoardCapabilities().batteryModelEvidenceLevel;
}

const char *nrfBoardUploadProfileEvidenceLevel() {
    return nrfBoardCapabilities().uploadProfileEvidenceLevel;
}

const char *nrfBoardClockSourceEvidenceLevel() {
    return nrfBoardCapabilities().clockSourceEvidenceLevel;
}

const char *nrfBoardLowFrequencyClockSource() {
    return nrfBoardCapabilities().lowFrequencyClockSource;
}

bool nrfBoardHasNativeUsb() {
    return nrfBoardCapabilities().hasNativeUsb;
}

bool nrfBoardHasBatterySense() {
    return nrfBoardCapabilities().hasBatterySense && (boardPinReachable(PIN_BATTERY) || nrfBoardCapabilities().batterySenseViaVddhDiv5);
}

bool nrfBoardHasQspiFlash() {
    return nrfBoardCapabilities().hasQspiFlash && qspiPinsReachable();
}

bool nrfBoardHasRgbLed() {
    return nrfBoardCapabilities().hasRgbLed && rgbPinsReachable();
}

bool nrfBoardHasWiFiCoprocessor() {
    return nrfBoardCapabilities().hasWiFiCoprocessor;
}

bool nrfBoardHasImu() {
    return nrfBoardCapabilities().hasImu;
}

bool nrfBoardHasNfc() {
    return nrfBoardCapabilities().hasNfc;
}

bool nrfBoardHasUsbCdc() {
    return nrfSystemProfile().hasUsbCdc;
}

bool nrfBoardHasSwdDebug() {
    return nrfSystemProfile().hasSwdDebug;
}

bool nrfBoardPinMapVerified() {
    return nrfBoardCapabilities().pinMapVerified;
}

bool nrfBoardBatteryModelVerified() {
    return nrfBoardCapabilities().batteryModelVerified;
}

bool nrfBoardUploadProfileVerified() {
    return nrfBoardCapabilities().uploadProfileVerified;
}

bool nrfBoardLowPowerProfileModeled() {
    return nrfBoardCapabilities().lowPowerProfileModeled && lowFrequencyClockDeclared();
}

bool nrfBoardBatterySenseViaVddhDiv5() {
    return nrfBoardCapabilities().batterySenseViaVddhDiv5;
}

uint8_t nrfBoardBatteryVoltageScaleNumerator() {
    if (nrfBoardCapabilities().batteryVoltageScaleNumerator == 0U) {
        return 1U;
    }
    return nrfBoardCapabilities().batteryVoltageScaleNumerator;
}

uint8_t nrfBoardBatteryVoltageScaleDenominator() {
    if (nrfBoardCapabilities().batteryVoltageScaleDenominator == 0U) {
        return 1U;
    }
    return nrfBoardCapabilities().batteryVoltageScaleDenominator;
}

bool nrfBoardUsbPowerPresent() {
    if (nrfBoardHasNativeUsb()) {
        return nrfUsbdDriver().status().vbusDetected;
    }
    return false;
}

bool nrfBoardUsbBatteryCoexistencePossible() {
    return nrfBoardHasNativeUsb() && nrfBoardHasBatterySense();
}

bool nrfBoardUsbBatteryCoexistenceActive() {
    return nrfBoardUsbBatteryCoexistencePossible() && nrfBoardUsbPowerPresent();
}

bool nrfBoardHasSecondarySpi() {
    return nrfBoardBusInfo().secondarySpiAvailable;
}

bool nrfBoardHasSecondaryWire() {
    return nrfBoardBusInfo().secondaryWireAvailable;
}

bool nrfBoardSpiDefaultPinsVerified() {
    return nrfBoardPinMapVerified() || boardFamilyEquals("promicro-compatible");
}

bool nrfBoardSerial1PinsVerified() {
    return nrfBoardPinMapVerified() || boardFamilyEquals("promicro-compatible");
}

bool nrfBoardSpi1PinsVerified() {
    return nrfBoardCapabilities().spi1PinsVerified;
}

bool nrfBoardWire1PinsVerified() {
    return nrfBoardCapabilities().wire1PinsVerified;
}

bool nrfBoardHasExtVccControl() {
    return boardPinReachable(PIN_EXT_VCC);
}

bool nrfBoardHasChargeStatus() {
    return boardPinReachable(PIN_CHARGE_STATUS);
}

bool nrfBoardPinValid(uint8_t pin) {
    return nrfPinInfo(pin).valid;
}

bool nrfBoardPinSupportsAnalogInput(uint8_t pin) {
    return nrfPinInfo(pin).analogInput;
}

bool nrfBoardPinSupportsPwm(uint8_t pin) {
    return nrfPinInfo(pin).pwmOutput;
}
