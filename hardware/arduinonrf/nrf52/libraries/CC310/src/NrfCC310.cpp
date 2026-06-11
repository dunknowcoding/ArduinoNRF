// NrfCC310.cpp - compatibility shim forwarding to NiusCrypto.
//
// Every call is delegated to the global `Crypto` object with Prefer::CC310.
// Same pattern as libraries/Thread/ → NiusThread.

#include "NrfCC310.h"

#include <NiusCrypto.h>

namespace {

bool g_started = false;

NrfCC310::Status mapStatus(ncrypto::CryptoStatus s) {
  using CS = ncrypto::CryptoStatus;
  switch (s) {
    case CS::Ok:
      return NrfCC310::CC_OK;
    case CS::NotStarted:
      return NrfCC310::CC_NOT_STARTED;
    case CS::BadParam:
      return NrfCC310::CC_BAD_PARAM;
    case CS::InternalError:
    case CS::AuthFailed:
      return NrfCC310::CC_INTERNAL;
    case CS::Unsupported:
    case CS::HardwareMissing:
    default:
      return NrfCC310::CC_NOT_VENDORED;
  }
}

#define CC310_GUARD()                          \
  do {                                         \
    if (!g_started) return CC_NOT_STARTED;     \
  } while (0)

}  // namespace

NrfCC310::Status NrfCC310::begin() {
  if (!Crypto.begin(CryptoEngine::Prefer::CC310)) {
    g_started = false;
    return CC_NOT_VENDORED;
  }
  g_started = true;
  return CC_OK;
}

void NrfCC310::end() {
  if (g_started) {
    Crypto.end();
    g_started = false;
  }
}

bool NrfCC310::isAvailable() {
  return g_started && Crypto.started();
}

NrfCC310::Status NrfCC310::randomBytes(uint8_t* buf, size_t len) {
  CC310_GUARD();
  if (buf == nullptr || len == 0) return CC_BAD_PARAM;
  return mapStatus(Crypto.random(buf, len));
}

NrfCC310::Status NrfCC310::sha256(const uint8_t* in, size_t inLen, uint8_t out[32]) {
  CC310_GUARD();
  if (in == nullptr || out == nullptr) return CC_BAD_PARAM;
  return mapStatus(Crypto.sha256(in, inLen, out));
}

NrfCC310::Status NrfCC310::aes128CbcEncrypt(const uint8_t key[16], const uint8_t iv[16],
                                            const uint8_t* in, uint8_t* out,
                                            size_t lenMultipleOf16) {
  CC310_GUARD();
  if ((lenMultipleOf16 & 0xFU) != 0U) return CC_BAD_PARAM;
  return mapStatus(Crypto.aesCbcEncrypt(key, iv, in, out, lenMultipleOf16));
}

NrfCC310::Status NrfCC310::aes128CbcDecrypt(const uint8_t key[16], const uint8_t iv[16],
                                            const uint8_t* in, uint8_t* out,
                                            size_t lenMultipleOf16) {
  CC310_GUARD();
  if ((lenMultipleOf16 & 0xFU) != 0U) return CC_BAD_PARAM;
  return mapStatus(Crypto.aesCbcDecrypt(key, iv, in, out, lenMultipleOf16));
}

NrfCC310::Status NrfCC310::aes128GcmEncrypt(const uint8_t key[16], const uint8_t iv[12],
                                            const uint8_t* aad, size_t aadLen,
                                            const uint8_t* in, uint8_t* out, size_t inLen,
                                            uint8_t tag[16]) {
  CC310_GUARD();
  return mapStatus(Crypto.aesGcmEncrypt(key, iv, aad, aadLen, in, out, inLen, tag));
}

NrfCC310::Status NrfCC310::aes128GcmDecrypt(const uint8_t key[16], const uint8_t iv[12],
                                           const uint8_t* aad, size_t aadLen,
                                           const uint8_t* in, uint8_t* out, size_t inLen,
                                           const uint8_t tag[16]) {
  CC310_GUARD();
  return mapStatus(Crypto.aesGcmDecrypt(key, iv, aad, aadLen, in, out, inLen, tag));
}

NrfCC310::Status NrfCC310::ecdsaP256Sign(const uint8_t privateKey[32],
                                         const uint8_t hash[32],
                                         uint8_t signature[64]) {
  CC310_GUARD();
  return mapStatus(Crypto.ecdsaSign(privateKey, hash, signature));
}

NrfCC310::Status NrfCC310::ecdsaP256Verify(const uint8_t publicKey[64],
                                          const uint8_t hash[32],
                                          const uint8_t signature[64]) {
  CC310_GUARD();
  return mapStatus(Crypto.ecdsaVerify(publicKey, hash, signature));
}

NrfCC310::Status NrfCC310::ecdhP256ComputeShared(const uint8_t privateKey[32],
                                                 const uint8_t peerPublicKey[64],
                                                 uint8_t sharedSecret[32]) {
  CC310_GUARD();
  return mapStatus(Crypto.ecdhShared(privateKey, peerPublicKey, sharedSecret));
}
