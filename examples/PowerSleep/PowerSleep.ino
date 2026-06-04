// PowerSleep.ino - the practical sleep / wake surface of NrfPower.
//
//   1. Reports why we woke up (POWER_RESETREAS) at boot.
//   2. Enables the main DCDC regulator (~30% lower run/idle current).
//   3. Switches to System ON low-power sub-mode.
//   4. Sleeps for 2 seconds via NrfPower::sleepMs() - CPU off, RTC1 wake.
//   5. Blinks LED_BUILTIN briefly.
//   6. After 5 cycles, demos GPIO wake by entering SystemOFF with a pin set
//      to wake on a falling edge. After wake the sketch starts over from
//      setup() and the reset-reason check shows RESET_GPIO_WAKE.
//
// Run with `examples/RtcAlarm` for the RTC-driven interrupt-wake pattern.

#include <NrfPower.h>

// Pick a GPIO that's safe to short to GND on your board. On the AliExpress
// ProMicro nRF52840 P0.13 is the user LED's anode but otherwise free; pick
// something not driven by your hardware.
constexpr uint8_t WAKE_PIN = 11;     // Arduino-numbered

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println(F("NrfPower demo"));
  const uint32_t rr = NrfPower::getResetReason();
  Serial.print(F("  RESETREAS=0x")); Serial.println(rr, HEX);
  if (NrfPower::wokeFromSystemOff()) {
    Serial.println(F("  -> we woke from SystemOFF"));
    if (rr & NrfPower::RESET_GPIO_WAKE) Serial.println(F("     by GPIO DETECT"));
    if (rr & NrfPower::RESET_NFC_WAKE)  Serial.println(F("     by NFC field"));
    if (rr & NrfPower::RESET_VBUS_WAKE) Serial.println(F("     by USB plug"));
  }
  NrfPower::clearResetReason();

  Serial.print(F("  DCDC enabled? ")); Serial.println(NrfPower::isDcdcEnabled());
  Serial.print(F("  VBUS present? "));  Serial.println(NrfPower::isUsbVbusPresent());

  // Enable main DCDC and switch to low-power sub-mode.
  NrfPower::enableDcdc(true);
  NrfPower::setMode(NrfPower::MODE_LOW_POWER);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(WAKE_PIN, INPUT_PULLUP);
}

void loop() {
  static uint8_t cycles = 0;

  // Blink for ~200 ms (the visible "we're alive" pulse).
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);

  // Sleep 2 seconds. CPU is off; RTC1 ticks at 1 kHz on LFCLK; wake on CC0.
  Serial.print(F("sleeping 2s ... cycle ")); Serial.println(cycles);
  Serial.flush();
  NrfPower::sleepMs(2000UL);

  ++cycles;
  if (cycles >= 5U) {
    Serial.print(F("entering SystemOFF, pull pin ")); Serial.print(WAKE_PIN);
    Serial.println(F(" LOW to wake (resets the chip)"));
    Serial.flush();
    NrfPower::enableGpioWake(WAKE_PIN, /*activeHigh=*/false);
    NrfPower::enterSystemOff();   // does not return
  }
}
