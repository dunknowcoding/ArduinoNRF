// PWMMultiModule.ino - showcase the multi-module PWM extensions.
//
// The nRF52840 has 4 PWM modules (PWM0..PWM3), each with 4 channels and an
// independent prescaler+countertop. This sketch puts up to 4 pins on different
// modules so each runs at its own frequency, sets a custom polarity, and
// configures a complementary pair with software dead-time.
//
// Watch the pins with a scope or LEDs. With the ProMicro-like variant the
// onboard RGB LED occupies LED_BUILTIN / LED_RED / LED_GREEN / LED_BLUE; swap
// them for whatever PWM-capable pins your board exposes.


void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}

  Serial.println(F("PWM multi-module demo"));
  Serial.print(F("channels total: "));
  Serial.println(nrfPwmChannelCapacity());      // 16 across PWM0..PWM3
  Serial.print(F("timer groups:   "));
  Serial.println(nrfPwmTimerGroupCount());      // 4 independent

  // --- four pins, four different frequencies ------------------------------
  // Each call allocates a free PWM module (if available) and sets its
  // prescaler + countertop independently. analogWrite() then writes a duty
  // cycle into the pin's group.
  const uint8_t pins[4]   = { LED_BUILTIN, LED_RED, LED_GREEN, LED_BLUE };
  const uint32_t freqs[4] = {   500UL,      2000UL,  10000UL,   50000UL };
  for (uint8_t i = 0; i < 4; ++i) {
    if (!nrfPwmSetPinFrequency(pins[i], freqs[i])) {
      Serial.print(F("setPinFrequency failed for pin ")); Serial.println(pins[i]);
      continue;
    }
    analogWrite(pins[i], 128);                   // 50% duty (8-bit default)
    Serial.print(F("pin "));    Serial.print(pins[i]);
    Serial.print(F(" group ")); Serial.print(nrfPwmPinTimerGroup(pins[i]));
    Serial.print(F(" @ "));     Serial.print(nrfPwmPinFrequencyHz(pins[i]));
    Serial.println(F(" Hz"));
  }

  // --- per-pin polarity inversion -----------------------------------------
  // Flip LED_RED's polarity so 0=full-on / 255=off (useful for active-low
  // LEDs, or for symmetric pairing).
  nrfPwmSetPinPolarity(LED_RED, NRF_PWM_PIN_POLARITY_LOW_ON_DUTY);

  // --- complementary half-bridge pair with software dead-time -------------
  // Pin A drives the high side, pin B is its phase-inverted complement;
  // dead-time ticks shave the on-time of B so they never overlap.
  // (Counter ticks depend on the module's prescaler / countertop - 4 ticks
  // is a safe minimum for the default 16 MHz / 1023 layout.)
  nrfPwmConfigureComplementary(LED_GREEN, LED_BLUE, /*deadTimeTicks=*/4);
}

void loop() {
  // Slow sweep on LED_BUILTIN to make the multi-frequency setup visible.
  static uint16_t duty = 0;
  static int8_t   step = 4;
  analogWrite(LED_BUILTIN, duty);
  duty = static_cast<uint16_t>(duty + step);
  if (duty == 0 || duty == 252) { step = -step; }
  delay(40);
}
