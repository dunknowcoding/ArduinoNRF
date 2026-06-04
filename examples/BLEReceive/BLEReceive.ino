// BLEReceive - receive data from a phone or PC over BLE.
//
// The board advertises a "Nordic UART Service" (a BLE UART). When a central
// (phone or PC) writes bytes to the RX characteristic, onReceive() runs and
// this sketch prints them to the USB Serial Monitor - like reading from a
// wireless Serial port.
//
// Test it with the free "nRF Connect" app (Android / iOS / desktop):
//   1. Upload this sketch (use a usbcdc=enabled board option for the monitor).
//   2. Open the Serial Monitor at 115200 baud.
//   3. In nRF Connect, SCAN and CONNECT to "ArduinoNRF-Recv".
//   4. Expand "Nordic UART Service", tap the write icon on the RX
//      characteristic (UUID 6E40...0002) and send some text.
//   5. The text appears in the Serial Monitor.
//
// No wiring needed - BLE is wireless. Just power the board over USB.
#include <NimBLE.h>

// Called whenever the central writes to the RX characteristic. This runs
// inside the BLE event pump (poll()), so keep it short and do not block here -
// printing to Serial is fine.
void onBleData(const uint8_t *data, size_t length) {
  Serial.print("BLE RX: ");
  Serial.write(data, length);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  NimBLE::onReceive(onBleData);       // register the handler before advertising
  NimBLE::begin("ArduinoNRF-Recv");
  NimBLE::startAdvertising();
}

void loop() {
  NimBLE::poll();                     // service the BLE stack - call this often
}
