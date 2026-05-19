// Applicable boards: all packaged boards.
// Limitations: validates the shared-timer PWM implementation and its user-facing frequency controls; no tone, servo, polarity, or center-aligned mode is expected.

#include <Arduino.h>

static const unsigned long SERIAL_WAIT_MS = 3000;
static const unsigned long REPORT_INTERVAL_MS = 1000;

void printRuntimeReport() {
  Serial.print("runtime frequency: ");
  Serial.println(analogWriteFrequencyHz());
  Serial.print("runtime counter top: ");
  Serial.println(nrfPwmCounterTop());
  Serial.print("runtime effective bits: ");
  Serial.println(nrfPwmEffectiveResolutionBits());
  Serial.print("runtime active channels: ");
  Serial.println(nrfPwmActiveChannels());
  Serial.print("runtime last status: ");
  Serial.println(static_cast<int>(nrfPwmLastWriteStatus()));
}

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < SERIAL_WAIT_MS) {
    delay(10);
  }
  const NrfBoardPowerInfo power = nrfBoardPowerInfo();
  uint8_t pwmPins[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t found = 0;

  for (uint8_t pin = 0; pin < PINS_COUNT && found < 5; ++pin) {
    if (digitalPinHasPWM(pin)) {
      pwmPins[found++] = pin;
    }
  }

  Serial.print("channel capacity: ");
  Serial.println(nrfPwmChannelCapacity());
  Serial.print("shared timer: ");
  printYesNo(nrfPwmSharedTimer());
  Serial.print("independent timers: ");
  printYesNo(nrfPwmIndependentTimersSupported());
  Serial.print("polarity configurable: ");
  printYesNo(nrfPwmPolarityConfigurable());
  Serial.print("center aligned: ");
  printYesNo(nrfPwmCenterAlignedSupported());
  Serial.print("frequency configurable: ");
  printYesNo(nrfPwmFrequencyConfigurable());
  Serial.print("minimum frequency: ");
  Serial.println(nrfPwmMinFrequencyHz());
  Serial.print("maximum frequency: ");
  Serial.println(nrfPwmMaxFrequencyHz());

  analogWriteResolution(15);
  analogWriteFrequency(50UL);
  Serial.print("frequency after 50 Hz request: ");
  Serial.println(analogWriteFrequencyHz());
  Serial.print("counter top at 50 Hz: ");
  Serial.println(nrfPwmCounterTop());
  Serial.print("effective bits at 50 Hz: ");
  Serial.println(nrfPwmEffectiveResolutionBits());

  analogWriteFrequency(1000UL);
  Serial.print("frequency after 1 kHz request: ");
  Serial.println(analogWriteFrequencyHz());
  Serial.print("counter top at 1 kHz: ");
  Serial.println(nrfPwmCounterTop());
  Serial.print("effective bits at 1 kHz: ");
  Serial.println(nrfPwmEffectiveResolutionBits());

  analogWriteFrequency(100000UL);
  Serial.print("frequency after 100 kHz request: ");
  Serial.println(analogWriteFrequencyHz());
  Serial.print("counter top at 100 kHz: ");
  Serial.println(nrfPwmCounterTop());
  Serial.print("effective bits at 100 kHz: ");
  Serial.println(nrfPwmEffectiveResolutionBits());

  analogWriteResolution(12);
  analogWriteFrequency(1000UL);
  Serial.print("tone supported: ");
  printYesNo(nrfToneSupported());
  Serial.print("servo supported: ");
  printYesNo(nrfServoSupported());

  if (found >= 5) {
    analogWrite(pwmPins[0], 100);
    analogWrite(pwmPins[1], 200);
    analogWrite(pwmPins[2], 300);
    analogWrite(pwmPins[3], 400);
    Serial.print("status after 4 channels: ");
    Serial.println(static_cast<int>(nrfPwmLastWriteStatus()));
    Serial.print("active channels: ");
    Serial.println(nrfPwmActiveChannels());

    analogWrite(pwmPins[4], 500);
    Serial.print("status after 5th write: ");
    Serial.println(static_cast<int>(nrfPwmLastWriteStatus()));
    Serial.print("active channels after 5th write: ");
    Serial.println(nrfPwmActiveChannels());
  }

  if (power.batterySensePin != 0xFF) {
    Serial.print("battery sense pin has PWM: ");
    printYesNo(digitalPinHasPWM(power.batterySensePin));
  }

  if (power.extVccControlPin != 0xFF) {
    Serial.print("ext VCC pin has PWM: ");
    printYesNo(digitalPinHasPWM(power.extVccControlPin));
  }
}

void loop() {
  static unsigned long lastReportMs = 0;
  const unsigned long now = millis();
  if ((now - lastReportMs) >= REPORT_INTERVAL_MS) {
    lastReportMs = now;
    printRuntimeReport();
  }
  delay(10);
}