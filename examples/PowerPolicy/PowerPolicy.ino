
void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.print("sleep supported: ");
  printYesNo(nrfSystemSleepSupported());
  Serial.print("power-down supported: ");
  printYesNo(nrfSystemPowerDownSupported());
  Serial.print("usb blocks low power: ");
  printYesNo(nrfSystemUsbBlocksLowPower());
  Serial.print("can sleep now: ");
  printYesNo(nrfSystemCanSleep());
  Serial.print("can power down now: ");
  printYesNo(nrfSystemCanPowerDown());
  Serial.print("usb/battery coexistence active: ");
  printYesNo(nrfUsbBatteryCoexistenceActive());
  Serial.print("usb/battery coexistence possible: ");
  printYesNo(nrfUsbBatteryCoexistencePossible());
}

void loop() {
  delay(50);
}