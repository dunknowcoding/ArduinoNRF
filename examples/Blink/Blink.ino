// Blink.ino - the classic. Blinks the built-in user LED at 1 Hz.
//
// On the AliExpress ProMicro / SuperMini nRF52840 the built-in LED is the
// ORANGE one on P0.15 (active-high) - hardware-verified via J-Link SWD. The
// blue LED on these boards is the LiPo charger's status LED and is NOT
// controllable from firmware.


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);   // LED on (active-high)
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);    // LED off
  delay(500);
}
