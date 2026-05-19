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
  NordicSelfTestReport report = {};
  bool passed = NordicHardware.runSelfTest(report);
  Serial.print("self-test passed: ");
  printYesNo(passed);
  Serial.print("reported reset reason: ");
  Serial.println(static_cast<int>(report.resetReason));
  Serial.print("current reset reason: ");
  Serial.println(static_cast<int>(NordicHardware.resetReason()));
}

void loop() {
  delay(100);
}
