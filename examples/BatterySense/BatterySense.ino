// Applicable boards: all packaged boards.
// Limitations: boards without declared battery sensing must report zero; boards with sensing validate the modeled conversion path only.

#include <NordicHardware.h>

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);
  const NrfBoardPowerInfo power = NordicHardware.boardPowerInfo();
  const uint32_t maxReading = (static_cast<uint32_t>(1) << nrfAdcConfiguredResolutionBits()) - 1;

  Serial.print("battery sense available: ");
  printYesNo(power.batterySenseAvailable);
  Serial.print("scale numerator: ");
  Serial.println(power.batteryVoltageScaleNumerator);
  Serial.print("scale denominator: ");
  Serial.println(power.batteryVoltageScaleDenominator);
  Serial.print("usb coexistence possible: ");
  printYesNo(power.usbBatteryCoexistencePossible);
  Serial.print("usb coexistence active: ");
  printYesNo(power.usbBatteryCoexistenceActive);

  if (power.batterySenseAvailable) {
    const int raw = NordicHardware.batteryRaw();
    const uint32_t mv = NordicHardware.batteryMillivolts();
    uint32_t clippedRaw = 0;
    if (raw > 0) {
      clippedRaw = static_cast<uint32_t>(raw);
    }
    const uint32_t sensedMillivolts = (clippedRaw * 3600 + (maxReading / 2)) / maxReading;
    const uint32_t expectedMillivolts =
      (sensedMillivolts * power.batteryVoltageScaleNumerator + (power.batteryVoltageScaleDenominator / 2)) /
      power.batteryVoltageScaleDenominator;

    Serial.print("raw reading: ");
    Serial.println(raw);
    Serial.print("millivolts: ");
    Serial.println(mv);
    Serial.print("expected millivolts: ");
    Serial.println(expectedMillivolts);

    if (power.batterySenseViaVddhDiv5) {
      Serial.print("VDDH/5 reading: ");
      Serial.println(analogReadVDDHDIV5());
    } else if (power.batterySensePin != 0xFF) {
      Serial.print("battery sense pin reading: ");
      Serial.println(analogRead(power.batterySensePin));
    }
  } else {
    Serial.print("raw reading: ");
    Serial.println(NordicHardware.batteryRaw());
    Serial.print("millivolts: ");
    Serial.println(NordicHardware.batteryMillivolts());
  }
}

void loop() {
  delay(50);
}
