// CC310Smoke.ino - sanity-check the CC310 compatibility shim.
//
// With NiusCrypto installed and Nordic binaries vendored (see
// NiusCrypto's docs/VENDORING.md), begin() succeeds and every forwarded
// primitive below passes. Without NiusCrypto the sketch fails to compile
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
    case NrfCC310::CC_AUTH_FAILED:
      return "AUTH_FAILED";
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

static bool reportOk(const char* label, NrfCC310::Status rc) {
  Serial.print(F("  "));
  Serial.print(label);
  Serial.print(F(": "));
  Serial.println(statusName(rc));
  return rc == NrfCC310::CC_OK;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}

  bool pass = true;
  Serial.println(F("=== CC310 shim smoke test ==="));
  const auto rc = NrfCC310::begin();
  pass &= reportOk("begin", rc);
  Serial.print(F("  isAvailable: "));
  Serial.println(NrfCC310::isAvailable() ? F("true") : F("false"));
  pass &= NrfCC310::isAvailable();

  uint8_t rnd[16] = {0};
  const auto rrc = NrfCC310::randomBytes(rnd, sizeof(rnd));
  pass &= reportOk("randomBytes", rrc);
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
  pass &= reportOk("sha256(\"abc\")", src);
  if (src == NrfCC310::CC_OK) {
    const bool shaOk = equal(digest, kShaAbc, 32);
    Serial.print(F("    match NIST: "));
    Serial.println(shaOk ? F("PASS") : F("FAIL"));
    pass &= shaOk;
  }

  static const uint8_t kHmacKey[4] = {'J', 'e', 'f', 'e'};
  static const uint8_t kHmacMsg[28] = {'w', 'h', 'a', 't', ' ', 'd', 'o',
                                       ' ', 'y', 'a', ' ', 'w', 'a', 'n',
                                       't', ' ', 'f', 'o', 'r', ' ', 'n',
                                       'o', 't', 'h', 'i', 'n', 'g', '?'};
  static const uint8_t kHmacMac[32] = {
      0x5b, 0xdc, 0xc1, 0x46, 0xbf, 0x60, 0x75, 0x4e, 0x6a, 0x04, 0x24,
      0x26, 0x08, 0x95, 0x75, 0xc7, 0x5a, 0x00, 0x3f, 0x08, 0x9d, 0x27,
      0x39, 0x83, 0x9d, 0xec, 0x58, 0xb9, 0x64, 0xec, 0x38, 0x43};
  uint8_t mac[32];
  const auto hrc = NrfCC310::hmacSha256(kHmacKey, sizeof(kHmacKey), kHmacMsg,
                                        sizeof(kHmacMsg), mac);
  pass &= reportOk("hmacSha256 (RFC 4231 #2)", hrc);
  if (hrc == NrfCC310::CC_OK) {
    const bool hmacOk = equal(mac, kHmacMac, 32);
    Serial.print(F("    match RFC 4231: "));
    Serial.println(hmacOk ? F("PASS") : F("FAIL"));
    pass &= hmacOk;
  }

  static const uint8_t kAesKey[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2,
                                      0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf,
                                      0x4f, 0x3c};
  static const uint8_t kCtrIv[16] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
                                     0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd,
                                     0xfe, 0xff};
  static const uint8_t kCtrPt[32] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e,
      0x11, 0x73, 0x93, 0x17, 0x2a, 0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03,
      0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51};
  static const uint8_t kCtrCt[32] = {
      0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26, 0x1b, 0xef, 0x68,
      0x64, 0x99, 0x0d, 0xb6, 0xce, 0x98, 0x06, 0xf6, 0x6b, 0x79, 0x70,
      0xfd, 0xff, 0x86, 0x17, 0x18, 0x7b, 0xb9, 0xff, 0xfd, 0xff};
  uint8_t ctrOut[32];
  const auto ctrc = NrfCC310::aes128Ctr(kAesKey, kCtrIv, kCtrPt, ctrOut, 32);
  pass &= reportOk("aes128Ctr (NIST F.5.1)", ctrc);
  if (ctrc == NrfCC310::CC_OK) {
    const bool ctrOk = equal(ctrOut, kCtrCt, 32);
    Serial.print(F("    match NIST: "));
    Serial.println(ctrOk ? F("PASS") : F("FAIL"));
    pass &= ctrOk;
  }

  uint8_t priv[32], pub[64], sig[64];
  const auto krc = NrfCC310::ecdsaP256GenerateKey(priv, pub);
  pass &= reportOk("ecdsaP256GenerateKey", krc);
  if (krc == NrfCC310::CC_OK) {
    uint8_t hash[32];
    const auto hrc2 = NrfCC310::sha256(kAbc, sizeof(kAbc), hash);
    pass &= reportOk("sha256 for ecdsa", hrc2);
    if (hrc2 == NrfCC310::CC_OK) {
      const auto src2 = NrfCC310::ecdsaP256Sign(priv, hash, sig);
      pass &= reportOk("ecdsaP256Sign", src2);
      if (src2 == NrfCC310::CC_OK) {
        const auto vrc = NrfCC310::ecdsaP256Verify(pub, hash, sig);
        pass &= reportOk("ecdsaP256Verify", vrc);
      }
    }
  }

  uint8_t aPriv[32], aPub[64], bPriv[32], bPub[64], sAB[32], sBA[32];
  const auto e1 = NrfCC310::ecdsaP256GenerateKey(aPriv, aPub);
  pass &= reportOk("ecdh party A keygen", e1);
  const auto e2 = NrfCC310::ecdsaP256GenerateKey(bPriv, bPub);
  pass &= reportOk("ecdh party B keygen", e2);
  if (e1 == NrfCC310::CC_OK && e2 == NrfCC310::CC_OK) {
    const auto e3 = NrfCC310::ecdhP256ComputeShared(aPriv, bPub, sAB);
    pass &= reportOk("ecdhP256ComputeShared A", e3);
    const auto e4 = NrfCC310::ecdhP256ComputeShared(bPriv, aPub, sBA);
    pass &= reportOk("ecdhP256ComputeShared B", e4);
    if (e3 == NrfCC310::CC_OK && e4 == NrfCC310::CC_OK) {
      const bool ecdhOk = equal(sAB, sBA, 32);
      Serial.print(F("    shared secret match: "));
      Serial.println(ecdhOk ? F("PASS") : F("FAIL"));
      pass &= ecdhOk;
    }
  }

  Serial.print(F("RESULT: "));
  Serial.println(pass ? F("OK") : F("CHECK"));
  Serial.flush();
}

void loop() {
  delay(5000);
}
