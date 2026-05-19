// Applicable boards: packaged nRF52 variants with a declared low-frequency clock source.
// This example follows the ArduinoBLE peripheral setup flow using the minimal advertising facade in this core.

#include <BLE.h>

BLEService deviceInfoService("180A");
BLECharacteristic manufacturerName("2A29", 0x02, 8);

void setup() {
  const byte vendor[] = {'A', 'r', 'd', 'u', 'i', 'n', 'o'};
  if (!BLE.begin()) {
    while (true) {
      delay(1000);
    }
  }

  BLE.setDeviceName("nRF52 GATT");
  BLE.setLocalName("nRF52 GATT");
  BLE.setAdvertisedService(deviceInfoService);

  if (!deviceInfoService.addCharacteristic(manufacturerName)) {
    while (true) {
      delay(1000);
    }
  }

  if (!manufacturerName.writeValue(vendor, sizeof(vendor))) {
    while (true) {
      delay(1000);
    }
  }

  if (!BLE.addService(deviceInfoService)) {
    while (true) {
      delay(1000);
    }
  }

  if (!BLE.advertise()) {
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  BLE.poll();
  delay(100);
}
