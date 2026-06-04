// UART - talk to other devices over the hardware serial pins.
//
// "Serial1" is the chip's hardware UART, wired to the silk-screen TX/RX pins.
// Use it to communicate with 3.3 V serial peripherals: GPS modules, sensors,
// another microcontroller, or a USB-to-serial adapter.
//
//   NOTE: Serial1 is NOT the USB Serial Monitor. The Monitor is "Serial"
//   (USB CDC). Serial1 moves bytes on the physical D0 / D1 pins.
//
// Wiring (3.3 V logic ONLY - 5 V will damage the nRF52840):
//   board D0 (TX)  ---->  peer RX
//   board D1 (RX)  <----  peer TX
//   board GND      -----  peer GND
//
// On the nRF52840 this hardware UART is driven by the UARTE peripheral
// (UART + EasyDMA). For everyday text you just use the familiar Serial API
// below; see the UARTE example for what EasyDMA adds.

void setup() {
  Serial1.begin(115200);            // 115200 baud, 8 data bits, no parity, 1 stop
  Serial1.println("UART ready");
}

void loop() {
  // Read one line from the peer (up to a newline) and echo it back.
  if (Serial1.available()) {
    String line = Serial1.readStringUntil('\n');
    Serial1.print("echo: ");
    Serial1.println(line);
  }
}
