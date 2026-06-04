// CC310Smoke.ino - sanity-check the CC310 library wiring.
//
// In the default (NRF_CC310_VENDORED=0) state every operation returns
// NrfCC310::NOT_VENDORED - that's expected, and this sketch prints exactly
// that so you know the library was found, included, and linked. After you
// drop libcc_310.a into vendor/lib/ and rebuild with -DNRF_CC310_VENDORED=1
// the same sketch starts returning real random bytes.

#include <NrfCC310.h>

const char *statusName(NrfCC310::Status s) {
  switch (s) {
    case NrfCC310::CC_OK:           return "OK";
    case NrfCC310::CC_NOT_VENDORED: return "NOT_VENDORED (drop libcc_310.a, see CC310/vendor/README.md)";
    case NrfCC310::CC_NOT_STARTED:  return "NOT_STARTED (begin() failed)";
    case NrfCC310::CC_BAD_PARAM:    return "BAD_PARAM";
    case NrfCC310::CC_INTERNAL:     return "INTERNAL";
    default:                        return "(unknown)";
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}

  Serial.println(F("CC310 smoke test"));
  const auto rc = NrfCC310::begin();
  Serial.print(F("  begin: ")); Serial.println(statusName(rc));
  Serial.print(F("  isAvailable: ")); Serial.println(NrfCC310::isAvailable());

  uint8_t rnd[16] = {0};
  const auto rrc = NrfCC310::randomBytes(rnd, sizeof(rnd));
  Serial.print(F("  randomBytes: ")); Serial.println(statusName(rrc));
  if (rrc == NrfCC310::CC_OK) {
    Serial.print(F("    bytes: "));
    for (size_t i = 0; i < sizeof(rnd); ++i) {
      if (rnd[i] < 0x10) Serial.print('0');
      Serial.print(rnd[i], HEX);
    }
    Serial.println();
  }
}

void loop() {
  delay(1000);
}
