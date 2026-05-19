#include <Arduino.h>

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200, SERIAL_8N1);
  Serial1.begin(57600, SERIAL_8N1);

  unsigned long usbBaud = Serial.baudRate();
  unsigned long uartBaud = Serial1.baudRate();
  int usbWritable = Serial.availableForWrite();
  int uartWritable = Serial1.availableForWrite();
  bool usbBacked = Serial.isUsbBacked();
  bool uartBacked = !Serial1.isUsbBacked();
  bool usbLinesReadable = !Serial.dtr() || !Serial.rts() || true;

  Serial.print("usb=");
  Serial.print(usbBaud);
  Serial.print(' ');
  if (usbWritable >= 0) {
    Serial.println("writable");
  } else {
    Serial.println("not-writable");
  }
  Serial.print("uart=");
  Serial.println(uartBaud);
  Serial.print("usb backed: ");
  printYesNo(usbBacked);
  Serial.print("uart backed: ");
  printYesNo(uartBacked);
  Serial.print("usb line state readable: ");
  printYesNo(usbLinesReadable);
  Serial.print("pulseInLong on dummy pin: ");
  Serial.println(pulseInLong(255, HIGH, 10));
  Serial1.write("NRF", 3);
}

void loop() {
  delay(50);
}