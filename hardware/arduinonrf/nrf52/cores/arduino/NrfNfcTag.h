// NrfNfcTag.h - NFC-A Type 2 tag emulation on the nRF52840 NFCT peripheral.
//
// Brings up the nRF52840 in passive NFC-A tag mode (13.56 MHz, ISO 14443-3
// Type A) and serves a small NDEF message to any NFC reader (smartphone,
// USB-NFC stick, NTAG-compatible reader). The tag is READ-ONLY by default -
// READ commands answer with the NDEF payload, WRITE commands NAK.
//
// What this covers:
//   * Auto-collision-resolution: NFCT's hardware ACR handles REQA / WUPA /
//     anticollision / SELECT, so the sketch only sees command frames after
//     the reader has selected our tag.
//   * Type 2 memory layout: 4-byte UID + BCC + CC (Capability Container)
//     + NDEF TLV. Big enough for a ~120-byte URL or short text record.
//   * NDEF URI / text builders so a sketch can write
//     `nrfNfcTag().beginUri("https://example.com")` and be done.
//   * Field-detect / field-lost / read-count counters so the sketch can
//     light an LED while the reader is in range and detect taps.
//   * Wakes from System ON sleep on FIELDDETECTED. Also acts as a
//     SystemOFF wake source if NrfPower::enableNfcWake() is called and the
//     tag is left active.
//
// What this DOES NOT cover (yet, see NIMBLE_INTEGRATION_PLAN.md style
// roadmap docs if you want them expanded):
//   * Type 4 tag (NDEF over ISO-DEP frames) - useful for longer messages
//     but ~10x the code.
//   * Writable tags / Type 2 WRITE command support.
//   * P2P (LLCP) - peer-to-peer is essentially never useful with phones any
//     more (deprecated on iOS and Android 10+).

#pragma once

#include <stdint.h>
#include <stddef.h>

typedef void (*nrfNfcCallback_t)(void);

class NrfNfcTag {
public:
    // The tag's RAM image is bounded - keep the NDEF message small enough
    // that the whole tag fits. 144 bytes after the static area (16 bytes)
    // is the practical ceiling for Type 2 on most readers.
    static constexpr size_t TAG_MEMORY_BYTES = 160U;
    static constexpr size_t NDEF_CAPACITY_BYTES = TAG_MEMORY_BYTES - 16U;

    // -- Bring-up ----------------------------------------------------------
    // beginUri()  - URI / URL (https, http, file, mailto, tel, ...). Common
    //               prefixes (http://, https://, https://www., tel:) are
    //               packed via the NDEF URI prefix codes so the payload is
    //               shorter on the wire. Returns false if the URL doesn't
    //               fit or NFCT is already busy.
    bool beginUri(const char *uri);

    // Plain-language text record. lang defaults to "en". Returns false on
    // overflow.
    bool beginText(const char *text, const char *lang = "en");

    // Raw NDEF payload - the bytes you pass in are placed in the NDEF TLV
    // after the 0x03 type byte and length. Useful for embedded MIME records
    // (Android Application Records, vCards, ...).
    bool beginRawNdef(const uint8_t *ndef, size_t length);

    // Stop tag emulation, disable the NFCT peripheral.
    void end();

    // -- State -------------------------------------------------------------
    bool isActive() const { return active_; }
    bool isFieldPresent() const;
    uint32_t fieldDetectCount() const { return fieldDetectCount_; }
    uint32_t fieldLostCount() const { return fieldLostCount_; }
    uint32_t readCount() const { return readCount_; }

    // -- Identity ----------------------------------------------------------
    // The chip ships with a unique NFC UID in FICR.NFC.TAGHEADERn. We use
    // it by default; setUid() lets a sketch override (e.g. to test against
    // a specific UID a reader is whitelisting). 4-byte UID is what fits
    // in the Type 2 first block (UID0..UID2 + BCC).
    void setUid(uint8_t b0, uint8_t b1, uint8_t b2);
    uint32_t uid() const;   // packed B0B1B2 in low 24 bits

    // -- Callbacks ---------------------------------------------------------
    // Run from the NFCT IRQ - keep them short. nullptr disables.
    void onFieldDetect(nrfNfcCallback_t cb) { fieldDetectCb_ = cb; }
    void onFieldLost(nrfNfcCallback_t cb)   { fieldLostCb_ = cb; }
    void onRead(nrfNfcCallback_t cb)        { readCb_ = cb; }

    // -- IRQ glue (called by NFCT_IRQHandler in NrfNfcTag.cpp) ------------
    void serviceIrq();

private:
    bool initPeripheral();
    void buildHeader();
    void buildCapabilityContainer(size_t ndefLen);
    bool buildUriPayload(uint8_t *out, size_t outCap, size_t *outLen, const char *uri);
    bool buildTextPayload(uint8_t *out, size_t outCap, size_t *outLen,
                           const char *text, const char *lang);
    bool finishBegin(size_t ndefLen);
    void handleCommand(uint8_t cmd, uint8_t arg);

    uint8_t tagMemory_[TAG_MEMORY_BYTES] = {0};
    uint8_t txBuffer_[64] = {0};
    uint8_t rxBuffer_[16] = {0};

    bool active_ = false;
    uint32_t fieldDetectCount_ = 0;
    uint32_t fieldLostCount_ = 0;
    uint32_t readCount_ = 0;

    nrfNfcCallback_t fieldDetectCb_ = nullptr;
    nrfNfcCallback_t fieldLostCb_ = nullptr;
    nrfNfcCallback_t readCb_ = nullptr;

    uint8_t userUid_[3] = {0, 0, 0};
    bool userUidSet_ = false;
};

// Singleton accessor - the IRQ dispatcher uses this.
NrfNfcTag &nrfNfcTag();
