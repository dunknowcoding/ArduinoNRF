volatile bool g_toggled = false;

void onButtonChange() {
  g_toggled = !g_toggled;
  if (g_toggled) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  attachInterrupt(PIN_BUTTON1, onButtonChange, CHANGE);
}

void loop() {
  delay(20);
}
