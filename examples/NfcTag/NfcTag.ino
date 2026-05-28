// NfcTag.ino - emulate an NFC Type 2 tag carrying a URL.
//
// Tap your phone to the antenna (the NFCT peripheral uses the NFC1 / NFC2
// pads - on a board WITHOUT an antenna connected, this still works at very
// short range against a phone's NFC sensor with the antenna pads touching).
// The phone should open https://github.com/dunknowcoding/ArduinoNRF.
//
// Counters print on Serial each time the field appears or a reader reads us.
//
// Hardware note: NFC1 (P0.09) and NFC2 (P0.10) double as GPIO. By default
// after flash they're configured as GPIO; FICR / UICR_NFCPINS gates this.
// Most ProMicro clones leave them as NFC by default. If you can't get a
// read, check NRF_UICR->NFCPINS or solder a small loop between P0.09 and
// P0.10 (with a tuning cap) to act as an antenna.

#include <Arduino.h>
#include <NrfNfcTag.h>

void onFieldDetect() {
  // ISR - keep short. Just toggle the LED.
  digitalWrite(LED_BUILTIN, HIGH);
}
void onFieldLost() {
  digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}
  pinMode(LED_BUILTIN, OUTPUT);

  NrfNfcTag &tag = nrfNfcTag();
  if (!tag.beginUri("https://github.com/dunknowcoding/ArduinoNRF")) {
    Serial.println(F("NFC tag begin failed"));
    return;
  }
  tag.onFieldDetect(onFieldDetect);
  tag.onFieldLost(onFieldLost);

  Serial.print(F("NFC tag active, UID="));
  Serial.println(tag.uid(), HEX);
  Serial.println(F("Tap with a phone."));
}

void loop() {
  static uint32_t lastReads = 0;
  static uint32_t lastFields = 0;
  const uint32_t reads = nrfNfcTag().readCount();
  const uint32_t fields = nrfNfcTag().fieldDetectCount();
  if (reads != lastReads || fields != lastFields) {
    lastReads = reads;
    lastFields = fields;
    Serial.print(F("fields=")); Serial.print(fields);
    Serial.print(F(", reads="));  Serial.println(reads);
  }
  delay(100);
}
