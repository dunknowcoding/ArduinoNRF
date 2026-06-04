// PWM.ino - fade an LED with analogWrite(), and print the resulting PWM config.
//
// analogWrite(pin, duty) works on ANY digital pin of the nRF52840 (the core
// exposes all 4 PWM modules / 16 channels). Here we fade the built-in LED;
// point `kFadePin` at any pad (e.g. D2) to drive an external LED instead.

static const uint8_t kFadePin = LED_BUILTIN;   // P0.15, the one on-board LED

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // 10-bit duty (0..1023). analogWriteFrequency() returns false if the carrier
  // cannot be produced at the current resolution.
  analogWriteResolution(10);
  if (!analogWriteFrequency(1000UL)) {       // ~1 kHz
    Serial.println("could not set 1 kHz PWM");
  }
  Serial.print("PWM frequency (Hz): ");
  Serial.println(analogWriteFrequencyHz());

  pinMode(kFadePin, OUTPUT);
}

void loop() {
  // Fade up then down. duty 0 = off, 1023 = full brightness (LED is active-high).
  for (int duty = 0; duty <= 1023; duty += 16) {
    analogWrite(kFadePin, duty);
    delay(4);
  }
  for (int duty = 1023; duty >= 0; duty -= 16) {
    analogWrite(kFadePin, duty);
    delay(4);
  }
}
