// Analog.ino - the minimal analog read: print the voltage on A0.
//
// A0..A2 are the analog-capable pads (A0 = D15 = P0.02). analogRead() returns
// 0..1023 by default (10-bit); call analogReadResolution(12) for 0..4095.
// For the full feature tour (reference, gain, calibration), see AnalogAdvanced.

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 1000UL) {
    delay(10);
  }
  pinMode(A0, INPUT);
}

void loop() {
  int value = analogRead(A0);
  Serial.print("A0 = ");
  Serial.println(value);
  delay(200);
}
