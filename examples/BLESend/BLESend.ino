// BLESend - send data to a phone or PC over BLE.
//
// The board advertises a "Nordic UART Service" (a BLE UART). Once a central
// (phone or PC) connects and subscribes, this sketch pushes a message every
// second as a TX notification - like printing to a wireless Serial port.
//
// Test it with the free "nRF Connect" app (Android / iOS / desktop):
//   1. Upload this sketch (use a usbcdc=enabled board option for the monitor).
//   2. In nRF Connect, SCAN and CONNECT to "ArduinoNRF-Send".
//   3. Expand "Nordic UART Service", then tap the notify icon on the TX
//      characteristic (UUID 6E40...0003) to subscribe.
//   4. Watch the "count=N" messages arrive once per second.
//
// No wiring needed - BLE is wireless. Just power the board over USB.
#include <NimBLE.h>

uint32_t counter = 0;
uint32_t lastSend = 0;

void setup() {
  Serial.begin(115200);
  NimBLE::begin("ArduinoNRF-Send");   // start the stack + set the advertised name
  NimBLE::startAdvertising();         // become connectable
}

void loop() {
  NimBLE::poll();                     // service the BLE stack - call this often

  // Once connected, send a line of text every second.
  if (NimBLE::isConnected() && millis() - lastSend >= 1000) {
    lastSend = millis();
    String msg = "count=";
    msg += counter++;
    msg += '\n';

    // write() sends the text as a TX notification. It returns 0 if no central
    // is connected or has not subscribed yet, which is fine to ignore here.
    NimBLE::write(msg.c_str());
  }
}
