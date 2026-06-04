// CryptoLpComp.ino - exercise the on-chip AES crypto block (ECB) and the
// low-power comparator (LPCOMP). Neither needs CC310 / NimBLE / a SoftDevice.
//
//   * Runs the ECB AES-128 hardware self-test against the FIPS-197 known
//     answer vector and prints PASS/FAIL.
//   * Encrypts a sample 16-byte block and prints the ciphertext hex.
//   * Configures LPCOMP on AIN0 (P0.02) at VDD*3/16 and prints whether the
//     pin is currently above the threshold every second. LPCOMP is the
//     comparator that keeps working in System OFF, so this same setup is what
//     you'd use as a deep-sleep analog wake source (pair with NrfPower).
//
// Wire AIN0 (P0.02) to a divider / sensor to watch the LPCOMP result change.

#include <NrfCrypto.h>
#include <NrfPeripherals.h>

static void printHex(const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (p[i] < 0x10) Serial.print('0');
    Serial.print(p[i], HEX);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println(F("CryptoLpComp"));

  // ---- ECB AES-128 hardware self-test ----
  Serial.print(F("  ECB AES-128 self-test (FIPS-197): "));
  Serial.println(NrfEcb::selfTest() ? F("PASS") : F("FAIL"));

  // Encrypt an arbitrary block with an arbitrary key.
  uint8_t key[16] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
  };
  uint8_t plain[16] = {
    0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
    0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
  };
  uint8_t cipher[16];
  if (NrfEcb::encrypt(key, plain, cipher)) {
    Serial.print(F("  ECB ciphertext: "));
    printHex(cipher, 16);   // expect 3ad77bb40d7a3660a89ecaf32466ef97
  } else {
    Serial.println(F("  ECB encrypt failed"));
  }

  // ---- LPCOMP on AIN0 (P0.02), threshold = VDD * 3/16 ----
  if (NrfLpComp::begin(NrfLpComp::AIN0, NrfLpComp::VDD_3_16, /*hysteresis=*/true)) {
    Serial.println(F("  LPCOMP armed on AIN0 @ VDD*3/16 (deep-sleep wake-capable)"));
  } else {
    Serial.println(F("  LPCOMP begin failed"));
  }
}

void loop() {
  Serial.print(F("  AIN0 above VDD*3/16? "));
  Serial.println(NrfLpComp::isAbove() ? F("yes") : F("no"));
  delay(1000);
}
