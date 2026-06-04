// Applicable boards: all packaged boards.
// Limitations: validates declared upload/runtime policy only; it does not exercise a real bootloader transition.


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

  Serial.print("debug available: ");
  printYesNo(debug.available());
  Serial.print("IDE ready: ");
  printYesNo(debug.ideReady());
  Serial.print("USB debug supported: ");
  printYesNo(debug.usbDebugSupported());
  Serial.print("probe required: ");
  printYesNo(debug.probeRequired());

#if defined(ARDUINO_NRF52_DEVBOARD_833)
  Serial.println("board profile: devboard_833");
#else
  Serial.println("board profile: native-usb target");

  if (system.bootloaderInterface == NrfBootloaderInterface::UsbUf2) {
    Serial.println("bootloader interface: UF2");
  } else {
    Serial.println("bootloader interface: DFU or other");
  }
#endif

  Serial.print("serial topology: ");
  Serial.println(static_cast<int>(system.serialTopology));
  Serial.print("runtime USB mode: ");
  Serial.println(static_cast<int>(system.runtimeUsbMode));
  Serial.print("monitor transport: ");
  Serial.println(static_cast<int>(system.monitorTransport));
  Serial.print("upload transport: ");
  Serial.println(static_cast<int>(system.uploadTransport));
  Serial.print("upload trigger: ");
  Serial.println(static_cast<int>(system.uploadTrigger));
  Serial.print("touch1200 declared: ");
  printYesNo(system.uploadTouch1200Declared);
  Serial.print("touch1200 verified: ");
  printYesNo(system.uploadTouch1200Verified);
  Serial.print("dfu alt setting: ");
  Serial.println(system.dfuAltSetting);
}

void loop() {
  delay(50);
}