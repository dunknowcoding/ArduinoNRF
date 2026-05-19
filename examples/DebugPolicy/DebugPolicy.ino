// Applicable boards: all packaged boards.
// Limitations: validates declared debug and upload policy only; it does not start a live debug session.

#include <Arduino.h>

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);
  const NrfSystemProfile &system = nrfSystemProfile();
  const NrfDebugConfig &debug = nrfDebugConfig();
  const bool usbStub = system.debugTransport == NrfDebugTransport::UsbCdcGdbStub;

  Serial.print("has SWD debug: ");
  printYesNo(system.hasSwdDebug);
  Serial.print("USB stub selected: ");
  printYesNo(usbStub);
  Serial.print("debug available: ");
  printYesNo(debug.available());
  Serial.print("IDE ready: ");
  printYesNo(debug.ideReady());
  Serial.print("USB debug supported: ");
  printYesNo(debug.usbDebugSupported());
  Serial.print("probe required: ");
  printYesNo(debug.probeRequired());
  Serial.print("connect under reset: ");
  printYesNo(debug.connectUnderReset);
  Serial.print("adapter speed (kHz): ");
  Serial.println(debug.adapterSpeedKhz);
  Serial.print("probe name: ");
  Serial.println(debug.probeName);
  Serial.print("transport name: ");
  Serial.println(debug.transportName);
  Serial.print("debug transport: ");
  Serial.println(static_cast<int>(system.debugTransport));
  Serial.print("upload transport: ");
  Serial.println(static_cast<int>(system.uploadTransport));
  Serial.print("upload trigger: ");
  Serial.println(static_cast<int>(system.uploadTrigger));
  Serial.print("monitor transport: ");
  Serial.println(static_cast<int>(system.monitorTransport));
}

void loop() {
  delay(50);
}