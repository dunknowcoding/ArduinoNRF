// Applicable boards: representative board-family targets, especially `promicro-compatible` variants.
// Limitations: asserts modeled board metadata and power-pin truth; it does not prove physical schematics beyond current declarations.

#include <NordicHardware.h>
#include <string.h>

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);
  NrfBoardInfo board = NordicHardware.boardInfo();
  NrfBoardSupportStatus support = NordicHardware.boardSupportStatus();
  NrfBoardPowerInfo power = NordicHardware.boardPowerInfo();
  NrfPinInfo batteryPin = {};
  NrfPinInfo extVccPin = {};
  if (power.batterySensePin != 0xFF) {
    batteryPin = NordicHardware.pinInfo(power.batterySensePin);
  }
  if (power.extVccControlPin != 0xFF) {
    extVccPin = NordicHardware.pinInfo(power.extVccControlPin);
  }

  Serial.print("board family: ");
  Serial.println(board.family);
  Serial.print("pin count: ");
  Serial.println(board.pinCount);
  Serial.print("battery sense available: ");
  printYesNo(power.batterySenseAvailable);
  Serial.print("battery via VDDH/5: ");
  printYesNo(power.batterySenseViaVddhDiv5);
  Serial.print("ext VCC control available: ");
  printYesNo(power.extVccControlAvailable);
  Serial.print("pin map verified: ");
  printYesNo(support.pinMapVerified);
  Serial.print("battery model verified: ");
  printYesNo(support.batteryModelVerified);
  Serial.print("low-power profile modeled: ");
  printYesNo(support.lowPowerProfileModeled);
  Serial.print("battery pin PWM capable: ");
  printYesNo(batteryPin.pwmOutput);
  Serial.print("ext VCC pin PWM capable: ");
  printYesNo(extVccPin.pwmOutput);
}

void loop() {
  delay(50);
}