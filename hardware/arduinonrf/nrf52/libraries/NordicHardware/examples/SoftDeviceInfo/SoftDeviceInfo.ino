/*
  SoftDeviceInfo - report the SoftDevice/MBR awareness of the bare-metal core.

  This core never starts a Nordic SoftDevice: NimBLE and Thread own the radio
  directly, and Zigbee uses an external CC2530.  A SoftDevice, when one is
  present in flash (e.g. the nice!nano ships an S140), therefore stays DORMANT
  and never claims a single peripheral - so RADIO / TIMER0 / RTC0 / the CCM,
  AAR and ECB crypto blocks all stay owned by your sketch.

  This sketch prints what the core detected at boot:
    * ProMicro nRF52840 clones (board1/board2/board3): "Absent" - the
      application is linked at 0x1000 with no SoftDevice region.
    * nice!nano v2 (board4/board5): "Dormant (S140 present, not started)" -
      app linked at 0x26000, SoftDevice present but unused.

  See docs/platform/SOFTDEVICE.md for the full architecture.
*/

#include <NordicHardware.h>
#include <NrfSoftDevice.h>

static const char *statusText(NrfSoftDevice::Status s) {
  switch (s) {
    case NrfSoftDevice::Status::Absent:  return "Absent (no SoftDevice in flash - pure bare-metal)";
    case NrfSoftDevice::Status::Dormant: return "Dormant (SoftDevice present in flash, never started)";
    case NrfSoftDevice::Status::Enabled: return "Enabled (started via the opt-in hook)";
  }
  return "unknown";
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
  }

  NordicSoftDeviceInfo info = NordicHardware.softDeviceInfo();

  Serial.println();
  Serial.println(F("=== ArduinoNRF SoftDevice awareness ==="));
  Serial.print(F("Chip          : "));
  Serial.println(NordicHardware.chipModel());
  Serial.print(F("Status        : "));
  Serial.println(statusText(NrfSoftDevice::status()));
  Serial.print(F("Present       : "));
  Serial.println(info.present ? F("yes") : F("no"));
  Serial.print(F("App start     : 0x"));
  Serial.println(info.appStartAddress, HEX);

  if (info.present) {
    Serial.print(F("SoftDevice base: 0x"));
    Serial.println(info.baseAddress, HEX);
    Serial.print(F("Firmware id   : 0x"));
    Serial.println(info.firmwareId, HEX);
    uint32_t v = info.versionRaw;
    Serial.print(F("Version       : "));
    Serial.print(v / 1000000UL);
    Serial.print('.');
    Serial.print((v / 1000UL) % 1000UL);
    Serial.print('.');
    Serial.println(v % 1000UL);
  }

  Serial.println();
  Serial.println(F("The core keeps the SoftDevice dormant: RADIO/TIMER0/RTC0/"));
  Serial.println(F("CCM/AAR/ECB stay owned by your sketch (what NimBLE & Thread"));
  Serial.println(F("require). requestEnable() is a guarded opt-in - off unless"));
  Serial.println(F("built with -DNRF_ENABLE_SOFTDEVICE and a vendored Nordic SDK."));
  Serial.print(F("requestEnable() now -> "));
  Serial.println(NrfSoftDevice::requestEnable() ? F("enabled") : F("blocked (expected on bare-metal)"));
}

void loop() {
  delay(1000);
}
