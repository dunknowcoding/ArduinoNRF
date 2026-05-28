# CryptoCell 310 integration plan — hardware crypto on nRF52840

## Scope & honest pacing

This document is the multi-session plan for wiring the **nRF52840 CryptoCell 310** hardware crypto accelerator (AES / SHA / RSA / ECC / TRNG / ChaCha20-Poly1305) to a clean Arduino API. The current state is a **skeleton library at `libraries/CC310/`** that compiles cleanly but returns `NOT_VENDORED` from every operation until the Nordic binary lib + headers are dropped in.

**About ARM TrustZone:** the request that originally pointed here was "ARM TrustZone CryptoCell 310". To be precise — **the nRF52840 is Cortex-M4F and does not have ARM TrustZone-M**. TrustZone-M lives on Cortex-M23/M33 (nRF5340, nRF9160). On the M4F, "secure isolation" comes from three separate features that this plan also covers:

1. **CC310** — independent crypto co-processor, key isolation via internal KDR / KCP slots that the CPU cannot read.
2. **APPROTECT** — debug-port lockout (UICR.APPROTECT = 0xFFFFFF00 in v1, sticky 0x0 / 0xFFFFFFFF in v2). Locks SWD from reading flash.
3. **ACL (Access Control List)** — up to 8 flash region locks that prevent CPU writes / instruction fetches. Useful for protecting a secure-boot loader.

The CC310 work is the largest chunk; APPROTECT + ACL are documented in stand-alone notes after the controller is in.

## Why this is multi-session

CC310 itself is straightforward — Nordic ships a precompiled library (`libcc_310.a`, ~600 KB total across symbols, ~30 KB linked footprint for a typical AES + SHA + ECDSA set). The work is in:

- Vendoring the lib + headers correctly per Nordic's binary license,
- Implementing the **platform abstraction** the lib expects (interrupt entry, RNG seeding glue, RAM context management, HFCLK request),
- Wrapping the C API (`SaSi_LibInit`, `CRYS_RND_Instantiate`, `CRYS_HASH_Update`, `CRYS_AES_Free`, …) in the small Arduino-shaped surface declared in `NrfCC310.h`,
- And — most consuming — writing **real-data verification** against test vectors (NIST FIPS 197 for AES, FIPS 180-4 for SHA, FIPS 186-4 for ECDSA) and against a host OpenSSL reference.

Sized in NimBLE-style milestones below.

## Milestones

### M1 — vendoring + first compile (1 session)

- Download the Nordic nRF5 SDK 17.x (free Nordic account required).
- Copy from the SDK:
  - `external/nrf_cc310/lib/cortex-m4/hard-float/no-interrupts/libcc_310_*.a` → `libraries/CC310/vendor/lib/libcc_310.a`
  - `external/nrf_cc310/include/*.h` → `libraries/CC310/vendor/include/`
- Add `extras.libcc310.flags=-DNRF_CC310_VENDORED=1 "-I{runtime.platform.path}/libraries/CC310/vendor/include" "-L{runtime.platform.path}/libraries/CC310/vendor/lib" -lcc_310` to `platform.txt` and thread it through `build.extra_flags`.
- Write `libraries/CC310/src/vendor_wiring.cpp` that defines `extern "C" nrfCC310_begin / nrfCC310_randomBytes / …` (the `extern` declarations already at the top of `NrfCC310.cpp` under `#if NRF_CC310_VENDORED`).
- Goal: **`libraries/CC310/examples/CC310Smoke` initializes the lib, prints `isAvailable()==true` over Serial, and reads 16 random bytes** through `randomBytes()` without faulting.

### M2 — symmetric crypto + hash (1 session)

- Wire AES-128 CBC encrypt/decrypt to `CRYS_AESCCM_Encrypt` (or the lower-level `CC_AES_Block`).
- Wire SHA-256 to `CRYS_HASH_Init / _Update / _Finish`.
- Verify against **NIST FIPS test vectors** (AES.KAT/CBCMMT128.rsp, SHA256ShortMsg.rsp). The `CC310Smoke` example grows into a self-check that runs the first 8 vectors of each and reports pass/fail.
- Add AES-128 GCM (authenticated AES) — Nordic's CC310 has a dedicated GCM path, ~5x faster than software.

### M3 — TRNG entropy + chacha20-poly1305 (1 session)

- Replace the stub `randomBytes` with `CRYS_RND_GenerateVector` using a properly instantiated DRBG seeded from CC310's TRNG.
- Surface entropy to mbedTLS and to Arduino's `random()` (optional — only if it doesn't break determinism guarantees in user sketches that called `randomSeed()`).
- ChaCha20-Poly1305 — single Nordic API call, useful for embedded protocols (Noise / WireGuard / SSH). 
- Verify against RFC 7539 / RFC 8439 test vectors.

### M4 — ECC: ECDSA P-256 sign+verify, ECDH P-256 (1 session)

- Most security-relevant operations for IoT (mutual TLS, code signing). Wire `CRYS_ECDSA_Sign / _Verify` and `CRYS_ECDH_SVDP_DH`.
- Generate test keypairs on host OpenSSL, sign on the device, verify on host (and vice-versa). All combinations must pass.
- Optionally add Curve25519 / Ed25519 (smaller code, similar API).

### M5 — secure key storage + APPROTECT + ACL (1 session)

- Use CC310's KCP (Key Customer Provisioned) slot to hold a per-device private key the CPU can't read.
- Document the APPROTECT v1 → v2 difference: nRF52840 chips manufactured after ~2022 use v2, which makes locking sticky. A wrong write to UICR.APPROTECT can permanently lock the chip — this needs SWD recovery procedure documentation **before** any sketch is shipped that touches APPROTECT.
- ACL config example: lock a 4-KB "secure boot" region so the app cannot overwrite it.

### M6 — Arduino BearSSL / mbedTLS bridge (1 session, optional)

- Hook the CC310 primitives into mbedTLS's `alt` slots so existing TLS libraries (mbedTLS, BearSSL) benefit transparently.
- Smoke test by running an HTTPS handshake against a public server with CC310 doing the heavy crypto. Useful for any Arduino sketch that does HTTPS.

## Conflicts & costs

- **HFCLK** must be active for CC310 to run — `begin()` calls `nrfStartHfclk()`. After `end()` we can drop back to LFCLK to save power.
- **Stack footprint** — CC310's lib expects ~4 KB of stack during ECDSA operations. Sketches with tight stacks need to bump.
- **Flash footprint** — typical AES+SHA256+ECDSA build adds ~25–35 KB to the binary. Fits comfortably in the ~800 KB user region.
- **No interrupt usage** — the `no-interrupts` library flavor we vendor is synchronous and busy-waits. The `interrupts` flavor (requires a Nordic-style scheduler) is out of scope.

## Status check

- M1: **not started** — skeleton library exists at `libraries/CC310/`; the binary lib is missing.
- M2–M6: **planned**.

## Source URLs

- Nordic CryptoCell 310 product page: <https://www.nordicsemi.com/Products/Development-software/nrf5-sdk/>
- nRF52840 PS chapter "CryptoCell 310": <https://docs.nordicsemi.com/bundle/ps_nrf52840/page/cryptocell.html>
- NIST FIPS test vectors: <https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program>
- RFC 7539 (ChaCha20-Poly1305): <https://datatracker.ietf.org/doc/html/rfc7539>
