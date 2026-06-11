// CC310Smoke.ino - sanity-check the CC310 compatibility shim.
//
// With NiusCrypto installed and Nordic binaries vendored (see
// NiusCrypto's docs/VENDORING.md), begin() succeeds and randomBytes /
// SHA-256("abc") pass. Without NiusCrypto the sketch fails to compile
// (same pattern as libraries/Thread/ → NiusThread).

#include <NrfCC310.h>

const char* statusName(NrfCC310::Status s) {
  switch (s) {
    case NrfCC310::CC_OK:
      return "OK";
    case NrfCC310::CC_NOT_VENDORED:
      return "NOT_VENDORED (install NiusCrypto + vendor binaries)";
    case NrfCC310::CC_NOT_STARTED:
      return "NOT_STARTED (begin() failed)";
    case NrfCC310::CC_BAD_PARAM:
      return "BAD_PARAM";
    case NrfCC310::CC_INTERNAL:
      return "INTERNAL";
    default:
      return "(unknown)";
  }
}

static bool equal(const uint8_t* a, const uint8_t* b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}

  Serial.println(F("=== CC310 shim smoke test ==="));
  const auto rc = NrfCC310::begin();
  Serial.print(F("  begin: "));
  Serial.println(statusName(rc));
  Serial.print(F("  isAvailable: "));
  Serial.println(NrfCC310::isAvailable() ? F("true") : F("false"));

  uint8_t rnd[16] = {0};
  const auto rrc = NrfCC310::randomBytes(rnd, sizeof(rnd));
  Serial.print(F("  randomBytes: "));
  Serial.println(statusName(rrc));
  if (rrc == NrfCC310::CC_OK) {
    Serial.print(F("    sample: "));
    for (size_t i = 0; i < sizeof(rnd); ++i) {
      if (rnd[i] < 0x10) Serial.print('0');
      Serial.print(rnd[i], HEX);
    }
    Serial.println();
  }

  static const uint8_t kAbc[] = {'a', 'b', 'c'};
  static const uint8_t kShaAbc[32] = {
      0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA, 0x41, 0x41, 0x40, 0xDE,
      0x5D, 0xAE, 0x22, 0x23, 0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
      0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD};
  uint8_t digest[32];
  const auto src = NrfCC310::sha256(kAbc, sizeof(kAbc), digest);
  Serial.print(F("  sha256(\"abc\"): "));
  Serial.println(statusName(src));
  if (src == NrfCC310::CC_OK) {
    Serial.print(F("    match NIST: "));
    Serial.println(equal(digest, kShaAbc, 32) ? F("PASS") : F("FAIL"));
  }

  Serial.print(F("RESULT: "));
  Serial.println((rc == NrfCC310::CC_OK && rrc == NrfCC310::CC_OK &&
                  src == NrfCC310::CC_OK && equal(digest, kShaAbc, 32))
                     ? F("OK")
                     : F("CHECK"));
  Serial.flush();
}

void loop() {
  delay(5000);
}
