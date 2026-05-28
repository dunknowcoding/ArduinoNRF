// NrfCC310.cpp - stub implementation. Returns NOT_VENDORED on every op
// until the Nordic libcc_310.a binary is dropped in (see vendor/README.md).
// Replace this file with the real wiring once the binary is present.

#include "NrfCC310.h"

// NRF_CC310_VENDORED - flip on via boards.txt or platform.txt when the
// binary library is available. Until then, every call returns NOT_VENDORED.

#ifndef NRF_CC310_VENDORED
#define NRF_CC310_VENDORED 0
#endif

#if NRF_CC310_VENDORED
// The real implementation will live in vendor_wiring.cpp (added when
// libcc_310.a is present). It links against the Nordic headers and forwards
// to SaSi_LibInit / CRYS_RND_GenerateVector / CRYS_HASH_Update / etc.
extern "C" {
extern NrfCC310::Status nrfCC310_begin();
extern void              nrfCC310_end();
extern NrfCC310::Status nrfCC310_randomBytes(uint8_t *, size_t);
extern NrfCC310::Status nrfCC310_sha256(const uint8_t *, size_t, uint8_t *);
extern NrfCC310::Status nrfCC310_aesCbcEncrypt(const uint8_t *, const uint8_t *,
                                                 const uint8_t *, uint8_t *, size_t);
extern NrfCC310::Status nrfCC310_aesCbcDecrypt(const uint8_t *, const uint8_t *,
                                                 const uint8_t *, uint8_t *, size_t);
extern NrfCC310::Status nrfCC310_aesGcmEncrypt(const uint8_t *, const uint8_t *,
                                                 const uint8_t *, size_t,
                                                 const uint8_t *, uint8_t *, size_t,
                                                 uint8_t *);
extern NrfCC310::Status nrfCC310_aesGcmDecrypt(const uint8_t *, const uint8_t *,
                                                 const uint8_t *, size_t,
                                                 const uint8_t *, uint8_t *, size_t,
                                                 const uint8_t *);
extern NrfCC310::Status nrfCC310_ecdsaP256Sign(const uint8_t *, const uint8_t *, uint8_t *);
extern NrfCC310::Status nrfCC310_ecdsaP256Verify(const uint8_t *, const uint8_t *, const uint8_t *);
extern NrfCC310::Status nrfCC310_ecdhP256(const uint8_t *, const uint8_t *, uint8_t *);
}
#endif

namespace {
bool g_started = false;
}

NrfCC310::Status NrfCC310::begin() {
#if NRF_CC310_VENDORED
    const Status r = nrfCC310_begin();
    g_started = (r == CC_OK);
    return r;
#else
    return CC_NOT_VENDORED;
#endif
}

void NrfCC310::end() {
#if NRF_CC310_VENDORED
    if (g_started) {
        nrfCC310_end();
        g_started = false;
    }
#endif
}

bool NrfCC310::isAvailable() {
#if NRF_CC310_VENDORED
    return g_started;
#else
    return false;
#endif
}

#define CC310_GUARD()                          \
    do {                                       \
        if (!g_started) return CC_NOT_STARTED;    \
    } while (0)

#if NRF_CC310_VENDORED

NrfCC310::Status NrfCC310::randomBytes(uint8_t *buf, size_t len) {
    CC310_GUARD();
    if (buf == nullptr || len == 0) return BAD_PARAM;
    return nrfCC310_randomBytes(buf, len);
}

NrfCC310::Status NrfCC310::sha256(const uint8_t *in, size_t inLen, uint8_t out[32]) {
    CC310_GUARD();
    if (in == nullptr || out == nullptr) return BAD_PARAM;
    return nrfCC310_sha256(in, inLen, out);
}

NrfCC310::Status NrfCC310::aes128CbcEncrypt(const uint8_t key[16], const uint8_t iv[16],
                                              const uint8_t *in, uint8_t *out, size_t lenMultipleOf16) {
    CC310_GUARD();
    if ((lenMultipleOf16 & 0xFU) != 0U) return BAD_PARAM;
    return nrfCC310_aesCbcEncrypt(key, iv, in, out, lenMultipleOf16);
}

NrfCC310::Status NrfCC310::aes128CbcDecrypt(const uint8_t key[16], const uint8_t iv[16],
                                              const uint8_t *in, uint8_t *out, size_t lenMultipleOf16) {
    CC310_GUARD();
    if ((lenMultipleOf16 & 0xFU) != 0U) return BAD_PARAM;
    return nrfCC310_aesCbcDecrypt(key, iv, in, out, lenMultipleOf16);
}

NrfCC310::Status NrfCC310::aes128GcmEncrypt(const uint8_t key[16], const uint8_t iv[12],
                                              const uint8_t *aad, size_t aadLen,
                                              const uint8_t *in, uint8_t *out, size_t inLen,
                                              uint8_t tag[16]) {
    CC310_GUARD();
    return nrfCC310_aesGcmEncrypt(key, iv, aad, aadLen, in, out, inLen, tag);
}

NrfCC310::Status NrfCC310::aes128GcmDecrypt(const uint8_t key[16], const uint8_t iv[12],
                                              const uint8_t *aad, size_t aadLen,
                                              const uint8_t *in, uint8_t *out, size_t inLen,
                                              const uint8_t tag[16]) {
    CC310_GUARD();
    return nrfCC310_aesGcmDecrypt(key, iv, aad, aadLen, in, out, inLen, tag);
}

NrfCC310::Status NrfCC310::ecdsaP256Sign(const uint8_t privateKey[32],
                                           const uint8_t hash[32],
                                           uint8_t signature[64]) {
    CC310_GUARD();
    return nrfCC310_ecdsaP256Sign(privateKey, hash, signature);
}

NrfCC310::Status NrfCC310::ecdsaP256Verify(const uint8_t publicKey[64],
                                             const uint8_t hash[32],
                                             const uint8_t signature[64]) {
    CC310_GUARD();
    return nrfCC310_ecdsaP256Verify(publicKey, hash, signature);
}

NrfCC310::Status NrfCC310::ecdhP256ComputeShared(const uint8_t privateKey[32],
                                                   const uint8_t peerPublicKey[64],
                                                   uint8_t sharedSecret[32]) {
    CC310_GUARD();
    return nrfCC310_ecdhP256(privateKey, peerPublicKey, sharedSecret);
}

#else  // NRF_CC310_VENDORED == 0

NrfCC310::Status NrfCC310::randomBytes(uint8_t *, size_t) { return CC_NOT_VENDORED; }
NrfCC310::Status NrfCC310::sha256(const uint8_t *, size_t, uint8_t *) { return CC_NOT_VENDORED; }
NrfCC310::Status NrfCC310::aes128CbcEncrypt(const uint8_t *, const uint8_t *, const uint8_t *, uint8_t *, size_t) { return CC_NOT_VENDORED; }
NrfCC310::Status NrfCC310::aes128CbcDecrypt(const uint8_t *, const uint8_t *, const uint8_t *, uint8_t *, size_t) { return CC_NOT_VENDORED; }
NrfCC310::Status NrfCC310::aes128GcmEncrypt(const uint8_t *, const uint8_t *, const uint8_t *, size_t, const uint8_t *, uint8_t *, size_t, uint8_t *) { return CC_NOT_VENDORED; }
NrfCC310::Status NrfCC310::aes128GcmDecrypt(const uint8_t *, const uint8_t *, const uint8_t *, size_t, const uint8_t *, uint8_t *, size_t, const uint8_t *) { return CC_NOT_VENDORED; }
NrfCC310::Status NrfCC310::ecdsaP256Sign(const uint8_t *, const uint8_t *, uint8_t *) { return CC_NOT_VENDORED; }
NrfCC310::Status NrfCC310::ecdsaP256Verify(const uint8_t *, const uint8_t *, const uint8_t *) { return CC_NOT_VENDORED; }
NrfCC310::Status NrfCC310::ecdhP256ComputeShared(const uint8_t *, const uint8_t *, uint8_t *) { return CC_NOT_VENDORED; }

#endif  // NRF_CC310_VENDORED
