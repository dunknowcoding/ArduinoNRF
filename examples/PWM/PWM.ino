
static const uint8_t PWM_PINS[] = {LED_BUILTIN, PIN_LED2, PIN_LED3};
static const uint8_t PWM_BITS = 10;
static const int PWM_MAX = 1023;
static const int PWM_STEP = 16;
static const unsigned long PWM_DELAY_MS = 4;

void printPwmConfig() {
  Serial.print("actual frequency: ");
  Serial.println(analogWriteFrequencyHz());
  Serial.print("counter clock: ");
  Serial.println(nrfPwmCounterClockHz());
  Serial.print("counter top: ");
  Serial.println(nrfPwmCounterTop());
  Serial.print("effective bits: ");
  Serial.println(nrfPwmEffectiveResolutionBits());
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 1000UL) {
    delay(10);
  }

  analogWriteResolution(PWM_BITS);
  if (!analogWriteFrequency(1000UL)) {
    Serial.println("failed to set PWM frequency");
  } else {
    Serial.println("PWM fade example");
    printPwmConfig();
  }

  for (uint8_t index = 0; index < sizeof(PWM_PINS); ++index) {
    pinMode(PWM_PINS[index], OUTPUT);
  }
}

void loop() {
  for (int value = 0; value <= PWM_MAX; value += PWM_STEP) {
    analogWrite(PWM_PINS[0], value);
    analogWrite(PWM_PINS[1], PWM_MAX - value);
    analogWrite(PWM_PINS[2], value / 2);
    delay(PWM_DELAY_MS);
  }

  for (int value = PWM_MAX; value >= 0; value -= PWM_STEP) {
    analogWrite(PWM_PINS[0], value);
    analogWrite(PWM_PINS[1], PWM_MAX - value);
    analogWrite(PWM_PINS[2], value / 2);
    delay(PWM_DELAY_MS);
  }
}