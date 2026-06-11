// NrfCC310.h - Arduino API surface for the nRF52840 CryptoCell 310 hardware
// crypto accelerator (Arm CryptoCell 310).
//
// COMPATIBILITY SHIM — forwards to NiusCrypto when installed:
//   https://github.com/dunknowcoding/ArduinoNRF-Crypto
//
// If the next compile fails with "NiusCrypto.h: No such file or directory",
// install the NiusCrypto library and vendor its Nordic binaries (see
// NiusCrypto's docs/VENDORING.md), then build again.
//
// SCOPE
//   This is a COMPATIBILITY SHIM. Working CC310 hardware crypto lives in the
//   separate NiusCrypto library (https://github.com/dunknowcoding/ArduinoNRF-Crypto).
//   The shim keeps `#include <NrfCC310.h>` compiling; without NiusCrypto the
//   sketch fails to compile — see vendor/README.md.
//
// IMPLEMENTED (via NiusCrypto when binaries are vendored)
//   TRNG, SHA-256, HMAC-SHA-256, AES-128 CBC/CTR/GCM, ECDSA/ECDH P-256.
//
// NOT EXPOSED (CryptoCell hardware can do more; future NiusCrypto roadmap)
//   SHA-1/224/384/512, ChaCha20-Poly1305, AES-CCM, RSA, Curve25519/Ed25519,
//   HKDF, PBKDF2, additional curves.
//
// IMPORTANT - CC310 is NOT ARM TrustZone
//   CryptoCell 310 is a separate crypto co-processor on the AHB bus. It is
//   often described together with TrustZone because Nordic's CryptoCell
//   product line also targets Cortex-M33 (where ARM TrustZone-M exists).
//   On Cortex-M4 nRF52840 there is no TrustZone - secure isolation here
//   comes from APPROTECT (debug-port lockout), CC310 key isolation, and
//   ACL flash region protection. See the integration roadmap doc for the
//   full security story.

#pragma once

#include <stdint.h>
#include <stddef.h>

class NrfCC310 {
public:
    // Status codes returned by every operation. CC_ prefix because Arduino.h
    // #define-s INTERNAL / DEFAULT as preprocessor macros that would otherwise
    // textually replace our enumerator names before the C++ compiler sees them.
    enum Status : int8_t {
        CC_OK            = 0,
        CC_NOT_VENDORED  = -1,  // NiusCrypto missing or CC310 backend unavailable
        CC_NOT_STARTED   = -2,  // begin() not called
        CC_BAD_PARAM     = -3,
        CC_INTERNAL      = -4,
        CC_AUTH_FAILED   = -5,  // signature/tag verification failed
    };

    // Initialize CC310 (HFCLK request, peripheral enable, RNG init).
    // Returns OK on real hardware; NOT_VENDORED if the binary lib is absent.
    static Status begin();
    static void   end();
    static bool   isAvailable();   // true only after a successful begin()

    // ---- Random ---------------------------------------------------------

    // Fill `buf` with `len` random bytes from the CC310 TRNG.
    static Status randomBytes(uint8_t *buf, size_t len);

    // ---- Hash -----------------------------------------------------------

    static Status sha256(const uint8_t *in, size_t inLen, uint8_t out[32]);
    static Status hmacSha256(const uint8_t *key, size_t keyLen,
                             const uint8_t *msg, size_t msgLen,
                             uint8_t out[32]);

    // ---- AES-128 --------------------------------------------------------

    static Status aes128CbcEncrypt(const uint8_t key[16],
                                    const uint8_t iv[16],
                                    const uint8_t *in,
                                    uint8_t *out,
                                    size_t lenMultipleOf16);
    static Status aes128CbcDecrypt(const uint8_t key[16],
                                    const uint8_t iv[16],
                                    const uint8_t *in,
                                    uint8_t *out,
                                    size_t lenMultipleOf16);
    static Status aes128Ctr(const uint8_t key[16],
                            const uint8_t iv[16],
                            const uint8_t *in,
                            uint8_t *out,
                            size_t len);

    // ---- Authenticated AES ----------------------------------------------

    static Status aes128GcmEncrypt(const uint8_t key[16],
                                    const uint8_t iv[12],
                                    const uint8_t *aad, size_t aadLen,
                                    const uint8_t *in, uint8_t *out, size_t inLen,
                                    uint8_t tag[16]);
    static Status aes128GcmDecrypt(const uint8_t key[16],
                                    const uint8_t iv[12],
                                    const uint8_t *aad, size_t aadLen,
                                    const uint8_t *in, uint8_t *out, size_t inLen,
                                    const uint8_t tag[16]);

    // ---- ECDSA P-256 sign / verify -------------------------------------

    static Status ecdsaP256GenerateKey(uint8_t privateKey[32],
                                       uint8_t publicKey[64]);
    static Status ecdsaP256Sign(const uint8_t privateKey[32],
                                 const uint8_t hash[32],
                                 uint8_t signature[64]);
    static Status ecdsaP256Verify(const uint8_t publicKey[64],
                                   const uint8_t hash[32],
                                   const uint8_t signature[64]);

    // ---- ECDH P-256 -----------------------------------------------------

    static Status ecdhP256ComputeShared(const uint8_t privateKey[32],
                                         const uint8_t peerPublicKey[64],
                                         uint8_t sharedSecret[32]);
};
