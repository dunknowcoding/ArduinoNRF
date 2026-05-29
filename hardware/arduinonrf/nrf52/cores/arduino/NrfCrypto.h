// NrfCrypto.h - bottom-level drivers for the nRF52840's on-chip AES crypto
// peripherals. These are the hardware blocks that sit OUTSIDE the CryptoCell
// 310 (which is a separate, binary-only accelerator handled by the CC310
// library). They need no vendored blob - they're plain memory-mapped
// peripherals like TIMER or RNG.
//
// Three peripherals live here:
//
//   * NrfEcb  - AES-128 ECB single-block encryption (ECB peripheral, IRQ 14).
//               General-purpose: hand it a 16-byte key + 16-byte block and it
//               returns the AES-128-encrypted block in ~7 us. This is the
//               building block for any AES mode you assemble in software
//               (CBC, CTR, CMAC, ...). Verified on hardware against the
//               FIPS-197 AES-128 test vector via NrfEcb::selfTest().
//
//   * NrfCcm  - AES-CCM packet encryption/decryption with MIC (CCM peripheral,
//               IRQ 15). This is the BLE link-layer cipher: it operates on
//               BLE PDU framing (the CNF structure carries the 128-bit key,
//               39-bit packet counter, direction bit and 8-byte IV). The
//               NimBLE controller (NIMBLE_INTEGRATION_PLAN M2) is its first
//               real consumer; the data formats follow the BLE spec exactly.
//
//   * NrfAar  - Accelerated Address Resolver (AAR peripheral, IRQ 15). Given a
//               BLE Resolvable Private Address and a list of IRKs, the
//               hardware tells you which IRK (if any) generated the address.
//               Used by a BLE host doing private-address resolution.
//
// IMPORTANT - peripheral conflicts:
//   * CCM and AAR share the SAME hardware AES core (base 0x4000F000, IRQ 15).
//     You can use one or the other at a time, never both concurrently. ECB
//     is independent (base 0x4000E000, IRQ 14).
//   * On the SoftDevice build profiles the SoftDevice owns ECB/CCM/AAR while
//     a BLE connection is encrypted. The verified promicroserialnosd path
//     leaves all three free for application use.
//
// All three APIs are blocking/polling: the operations complete in single-
// digit microseconds so spinning on the END event is simpler and faster than
// arming the NVIC. No vector-table entries are needed.

#pragma once

#include <stdint.h>
#include <stddef.h>

// ---- NrfEcb - AES-128 ECB single block -------------------------------------

class NrfEcb {
public:
    // Encrypt one 16-byte block with a 16-byte key. key/in/out are big-endian
    // (key[0] and in[0] are the most-significant bytes, matching the AES spec
    // and the nRF ECBDATA layout). `out` may alias `in`. Returns false only if
    // the hardware raises ERRORECB (an EasyDMA fault, e.g. a non-RAM pointer)
    // or times out.
    static bool encrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

    // Self-test against the FIPS-197 Appendix C.1 AES-128 vector:
    //   key   = 000102030405060708090a0b0c0d0e0f
    //   plain = 00112233445566778899aabbccddeeff
    //   ciph  = 69c4e0d86a7b0430d8cdb78070b4c55a
    // Returns true if the hardware reproduces the known-answer ciphertext.
    static bool selfTest();
};

// ---- NrfCcm - AES-CCM BLE packet cipher ------------------------------------

class NrfCcm {
public:
    enum Mode : uint8_t {
        ENCRYPT = 0,   // plaintext PDU in, encrypted PDU + MIC out
        DECRYPT = 1,   // encrypted PDU + MIC in, plaintext PDU out (+ MIC check)
    };

    enum DataRate : uint8_t {
        RATE_1MBIT   = 0,
        RATE_2MBIT   = 1,
        RATE_125KBPS = 2,   // BLE coded PHY (long range)
        RATE_500KBPS = 3,
    };

    // The 33-byte CCM CNF structure the hardware reads via CNFPTR. Layout is
    // fixed by the nRF52840 PS (CCM chapter): little-endian fields, packed.
    struct __attribute__((packed)) Config {
        uint8_t  key[16];        // session key (note: little-endian per PS)
        uint8_t  packetCounter[5];   // 39-bit counter, LSB first (5 bytes used)
        uint8_t  direction;      // bit0: 1 = master->slave, 0 = slave->master
        uint8_t  iv[8];          // initialization vector
    };

    // Enable the peripheral and point it at a caller-owned CNF structure plus a
    // scratch area. `scratch` must be at least (maxPduLength + 16) bytes of RAM
    // the hardware can scribble on. Call once before crypt().
    static void begin(Mode mode, DataRate rate, Config *cnf,
                      uint8_t *scratch, size_t scratchLen);
    static void end();

    // Encrypt or decrypt one BLE PDU. `inPdu` / `outPdu` are full link-layer
    // PDUs (1 byte header S0, 1 byte length, then payload[, MIC on decrypt]).
    // On ENCRYPT, outPdu gets payload+4-byte MIC appended. On DECRYPT, the
    // payload is decrypted and the MIC verified - micOk() reports the result.
    // Returns false on hardware ERROR or timeout.
    static bool crypt(const uint8_t *inPdu, uint8_t *outPdu);

    // After a DECRYPT crypt(): true if the message authentication code matched.
    static bool micOk();
};

// ---- NrfAar - BLE resolvable private address resolver ----------------------

class NrfAar {
public:
    static constexpr uint8_t MAX_IRKS = 16U;

    // Provide the IRK list: `nIrks` keys of 16 bytes each (little-endian, as
    // stored by a BLE host). The list is referenced by the hardware, so it
    // must stay valid until end(). Enables the peripheral.
    static void begin(const uint8_t *irks, uint8_t nIrks);
    static void end();

    // Try to resolve a 6-byte BLE address (as received over the air, LSB
    // first - the low 3 bytes are `hash`, the high 3 bytes are `prand`).
    // Returns the index 0..nIrks-1 of the matching IRK, or -1 if none matched
    // (or on hardware ERROR / timeout).
    static int resolve(const uint8_t address[6]);
};
