// NrfNfcTag.cpp - implementation. See NrfNfcTag.h.

#include "NrfNfcTag.h"
#include <string.h>

namespace {

constexpr uint32_t NFCT_BASE = 0x40005000UL;

// Tasks
constexpr uint32_t NFCT_TASKS_ACTIVATE      = 0x000UL;
constexpr uint32_t NFCT_TASKS_DISABLE       = 0x024UL;
constexpr uint32_t NFCT_TASKS_SENSE         = 0x028UL;
constexpr uint32_t NFCT_TASKS_STARTTX       = 0x004UL;
constexpr uint32_t NFCT_TASKS_ENABLERXDATA  = 0x00CUL;
// Events
constexpr uint32_t NFCT_EVENTS_READY         = 0x100UL;
constexpr uint32_t NFCT_EVENTS_FIELDDETECTED = 0x104UL;
constexpr uint32_t NFCT_EVENTS_FIELDLOST     = 0x108UL;
constexpr uint32_t NFCT_EVENTS_TXFRAMESTART  = 0x10CUL;
constexpr uint32_t NFCT_EVENTS_TXFRAMEEND    = 0x110UL;
constexpr uint32_t NFCT_EVENTS_RXFRAMESTART  = 0x114UL;
constexpr uint32_t NFCT_EVENTS_RXFRAMEEND    = 0x118UL;
constexpr uint32_t NFCT_EVENTS_ERROR         = 0x11CUL;
constexpr uint32_t NFCT_EVENTS_SELECTED      = 0x128UL;
// Registers
constexpr uint32_t NFCT_SHORTS         = 0x200UL;
constexpr uint32_t NFCT_INTENSET       = 0x304UL;
constexpr uint32_t NFCT_INTENCLR       = 0x308UL;
constexpr uint32_t NFCT_ERRORSTATUS    = 0x404UL;
constexpr uint32_t NFCT_NFCTAGSTATE    = 0x410UL;
constexpr uint32_t NFCT_FIELDPRESENT   = 0x42CUL;
constexpr uint32_t NFCT_FRAMEDELAYMAX  = 0x50CUL;
constexpr uint32_t NFCT_PACKETPTR      = 0x510UL;
constexpr uint32_t NFCT_MAXLEN         = 0x514UL;
constexpr uint32_t NFCT_TX_AMOUNT      = 0x518UL;
constexpr uint32_t NFCT_RX_AMOUNT      = 0x51CUL;
constexpr uint32_t NFCT_NFCID1_LAST      = 0x59CUL;   // bytes 4..7 of UID
constexpr uint32_t NFCT_NFCID1_2ND_LAST  = 0x598UL;
constexpr uint32_t NFCT_NFCID1_3RD_LAST  = 0x594UL;
constexpr uint32_t NFCT_SENSRES        = 0x534UL;
constexpr uint32_t NFCT_SELRES         = 0x540UL;

// SHORTS / INTEN bits
constexpr uint32_t NFCT_SHORTS_FIELDDETECTED_ACTIVATE = 1U << 0;
constexpr uint32_t NFCT_SHORTS_FIELDLOST_SENSE        = 1U << 1;
constexpr uint32_t NFCT_INTEN_FIELDDETECTED = 1U << 1;
constexpr uint32_t NFCT_INTEN_FIELDLOST     = 1U << 2;
constexpr uint32_t NFCT_INTEN_RXFRAMEEND    = 1U << 6;
constexpr uint32_t NFCT_INTEN_TXFRAMEEND    = 1U << 4;
constexpr uint32_t NFCT_INTEN_ERROR         = 1U << 7;
constexpr uint32_t NFCT_INTEN_SELECTED      = 1U << 10;

// FICR (factory info) - NFC UID source
constexpr uint32_t FICR_BASE              = 0x10000000UL;
constexpr uint32_t FICR_NFC_TAGHEADER0    = 0x450UL;
constexpr uint32_t FICR_NFC_TAGHEADER1    = 0x454UL;
constexpr uint32_t FICR_NFC_TAGHEADER2    = 0x458UL;
constexpr uint32_t FICR_NFC_TAGHEADER3    = 0x45CUL;

// NVIC
constexpr uint32_t NVIC_BASE  = 0xE000E000UL;
constexpr uint32_t NVIC_ISER0 = 0x100UL;
constexpr uint32_t NVIC_ICER0 = 0x180UL;
constexpr uint8_t  NFCT_IRQ_NUMBER = 5U;

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

inline void enableNvic(uint8_t irq) {
    reg32(NVIC_BASE, NVIC_ISER0 + ((irq >> 5U) * 4UL)) = 1UL << (irq & 0x1FU);
}

inline void disableNvic(uint8_t irq) {
    reg32(NVIC_BASE, NVIC_ICER0 + ((irq >> 5U) * 4UL)) = 1UL << (irq & 0x1FU);
}

// NDEF URI well-known prefix codes (NFC Forum URI RTD)
struct UriPrefix {
    uint8_t code;
    const char *prefix;
};
const UriPrefix kUriPrefixes[] = {
    {0x01, "http://www."},
    {0x02, "https://www."},
    {0x03, "http://"},
    {0x04, "https://"},
    {0x05, "tel:"},
    {0x06, "mailto:"},
    // ... extend if needed
};

uint8_t pickUriPrefixCode(const char **uri) {
    for (const auto &entry : kUriPrefixes) {
        const size_t plen = strlen(entry.prefix);
        if (strncmp(*uri, entry.prefix, plen) == 0) {
            *uri += plen;
            return entry.code;
        }
    }
    return 0x00;   // 0 = no prefix substitution
}

}  // namespace

NrfNfcTag &nrfNfcTag() {
    static NrfNfcTag instance;
    return instance;
}

// -- API ---------------------------------------------------------------------

bool NrfNfcTag::beginUri(const char *uri) {
    if (uri == nullptr) return false;
    uint8_t payload[NDEF_CAPACITY_BYTES];
    size_t payloadLen = 0;
    if (!buildUriPayload(payload, sizeof(payload), &payloadLen, uri)) {
        return false;
    }

    // Wrap the URI payload into an NDEF record. For TYPE "U" (1 byte) with
    // SR (Short Record) flag, the record header is 4 bytes:
    //   byte 0: 0xD1 = MB | ME | SR | TNF=1 (Well-known)
    //   byte 1: 0x01 (type length)
    //   byte 2: payload length
    //   byte 3: 'U' (type)
    if (payloadLen + 4U > NDEF_CAPACITY_BYTES - 4U) return false;

    uint8_t ndef[NDEF_CAPACITY_BYTES];
    ndef[0] = 0xD1U;
    ndef[1] = 0x01U;
    ndef[2] = static_cast<uint8_t>(payloadLen);
    ndef[3] = 'U';
    memcpy(&ndef[4], payload, payloadLen);
    return beginRawNdef(ndef, payloadLen + 4U);
}

bool NrfNfcTag::beginText(const char *text, const char *lang) {
    if (text == nullptr) return false;
    if (lang == nullptr) lang = "en";

    uint8_t payload[NDEF_CAPACITY_BYTES];
    size_t payloadLen = 0;
    if (!buildTextPayload(payload, sizeof(payload), &payloadLen, text, lang)) {
        return false;
    }

    // Record header: 0xD1 (MB|ME|SR|TNF=1), 0x01 (type len), payloadLen, 'T'
    if (payloadLen + 4U > NDEF_CAPACITY_BYTES - 4U) return false;

    uint8_t ndef[NDEF_CAPACITY_BYTES];
    ndef[0] = 0xD1U;
    ndef[1] = 0x01U;
    ndef[2] = static_cast<uint8_t>(payloadLen);
    ndef[3] = 'T';
    memcpy(&ndef[4], payload, payloadLen);
    return beginRawNdef(ndef, payloadLen + 4U);
}

bool NrfNfcTag::beginRawNdef(const uint8_t *ndef, size_t length) {
    if (ndef == nullptr || length == 0U) return false;
    if (length + 4U > NDEF_CAPACITY_BYTES) return false;   // need 0x03/len + 0xFE

    if (active_) {
        end();
    }

    // Header (UID + BCC) at blocks 0..2
    buildHeader();

    // CC at block 3
    buildCapabilityContainer(length);

    // NDEF TLV at block 4 onward
    //   0x03 (NDEF magic), length byte, ndef bytes..., 0xFE (terminator)
    tagMemory_[16] = 0x03U;
    tagMemory_[17] = static_cast<uint8_t>(length);
    memcpy(&tagMemory_[18], ndef, length);
    tagMemory_[18 + length] = 0xFEU;

    return finishBegin(length);
}

void NrfNfcTag::end() {
    reg32(NFCT_BASE, NFCT_INTENCLR) = 0xFFFFFFFFUL;
    reg32(NFCT_BASE, NFCT_TASKS_DISABLE) = 1UL;
    disableNvic(NFCT_IRQ_NUMBER);
    active_ = false;
}

bool NrfNfcTag::isFieldPresent() const {
    return (reg32(NFCT_BASE, NFCT_FIELDPRESENT) & 1U) != 0U;
}

void NrfNfcTag::setUid(uint8_t b0, uint8_t b1, uint8_t b2) {
    userUid_[0] = b0; userUid_[1] = b1; userUid_[2] = b2;
    userUidSet_ = true;
}

uint32_t NrfNfcTag::uid() const {
    if (userUidSet_) {
        return (static_cast<uint32_t>(userUid_[0]) << 16) |
               (static_cast<uint32_t>(userUid_[1]) << 8)  |
               (static_cast<uint32_t>(userUid_[2]));
    }
    // Pull from FICR if not overridden.
    const uint32_t fh0 = reg32(FICR_BASE, FICR_NFC_TAGHEADER0);
    return fh0 & 0x00FFFFFFUL;
}

// -- Internal helpers --------------------------------------------------------

void NrfNfcTag::buildHeader() {
    // Type 2 block 0 (bytes 0..3): UID0..UID2 + BCC0 = UID0^UID1^UID2^0x88
    uint8_t u0, u1, u2;
    if (userUidSet_) {
        u0 = userUid_[0]; u1 = userUid_[1]; u2 = userUid_[2];
    } else {
        const uint32_t fh0 = reg32(FICR_BASE, FICR_NFC_TAGHEADER0);
        u0 = static_cast<uint8_t>(fh0 & 0xFFU);
        u1 = static_cast<uint8_t>((fh0 >> 8) & 0xFFU);
        u2 = static_cast<uint8_t>((fh0 >> 16) & 0xFFU);
    }
    // For a 7-byte UID the first byte must be 0x88 (cascade tag). We use a
    // 4-byte UID for simplicity - the first byte is the lookalike-manufacturer
    // byte (0x04 = NXP, fine for tag emulation).
    tagMemory_[0] = 0x04U;        // Manufacturer
    tagMemory_[1] = u0;
    tagMemory_[2] = u1;
    tagMemory_[3] = static_cast<uint8_t>(0x04U ^ u0 ^ u1 ^ u2 ^ 0x88U);
    tagMemory_[4] = u2;
    tagMemory_[5] = 0x00U; tagMemory_[6] = 0x00U; tagMemory_[7] = 0x00U;
    tagMemory_[8] = 0x00U; tagMemory_[9] = 0x00U; tagMemory_[10] = 0x00U; tagMemory_[11] = 0x00U;
}

void NrfNfcTag::buildCapabilityContainer(size_t /*ndefLen*/) {
    // Block 3 = Capability Container
    //   byte 0: 0xE1 (NDEF magic)
    //   byte 1: 0x10 (version 1.0, MLEN granularity 8)
    //   byte 2: tag memory size in 8-byte chunks -> 16 = 128 bytes
    //   byte 3: 0x00 (read-only would be 0x0F)
    tagMemory_[12] = 0xE1U;
    tagMemory_[13] = 0x10U;
    tagMemory_[14] = 0x10U;
    tagMemory_[15] = 0x00U;
}

bool NrfNfcTag::buildUriPayload(uint8_t *out, size_t outCap, size_t *outLen, const char *uri) {
    const char *suffix = uri;
    const uint8_t code = pickUriPrefixCode(&suffix);
    const size_t slen = strlen(suffix);
    if (slen + 1U > outCap) return false;
    out[0] = code;
    memcpy(&out[1], suffix, slen);
    *outLen = slen + 1U;
    return true;
}

bool NrfNfcTag::buildTextPayload(uint8_t *out, size_t outCap, size_t *outLen,
                                  const char *text, const char *lang) {
    const size_t llen = strlen(lang);
    const size_t tlen = strlen(text);
    if (llen > 63U) return false;   // status byte's lang-len field is 6 bits
    if (llen + tlen + 1U > outCap) return false;
    out[0] = static_cast<uint8_t>(llen);   // UTF-8 (bit 7 = 0), lang length
    memcpy(&out[1], lang, llen);
    memcpy(&out[1 + llen], text, tlen);
    *outLen = llen + tlen + 1U;
    return true;
}

bool NrfNfcTag::finishBegin(size_t /*ndefLen*/) {
    if (!initPeripheral()) {
        return false;
    }
    // Hook the TX buffer to our tag memory so READ responses come from it.
    // We arm the RX path here; TX is filled per-command in serviceIrq().
    reg32(NFCT_BASE, NFCT_PACKETPTR) = reinterpret_cast<uint32_t>(rxBuffer_);
    reg32(NFCT_BASE, NFCT_MAXLEN) = sizeof(rxBuffer_);
    // ACTIVATE the analog block - it'll auto-arm sense and wait for a field.
    reg32(NFCT_BASE, NFCT_TASKS_ACTIVATE) = 1UL;
    active_ = true;
    return true;
}

bool NrfNfcTag::initPeripheral() {
    // Make sure we start from a clean state.
    reg32(NFCT_BASE, NFCT_TASKS_DISABLE) = 1UL;
    reg32(NFCT_BASE, NFCT_INTENCLR) = 0xFFFFFFFFUL;

    // SENS_RES = 0x0044 (NFC Forum Type 2). SEL_RES = 0x00 (NFC Forum Type 2).
    reg32(NFCT_BASE, NFCT_SENSRES) = 0x0044UL;
    reg32(NFCT_BASE, NFCT_SELRES) = 0x00UL;

    // FRAMEDELAYMAX - reader-frame-to-our-response upper bound.
    // 0x1000 = 4096 us, plenty.
    reg32(NFCT_BASE, NFCT_FRAMEDELAYMAX) = 0x1000UL;

    // Auto-collision: FIELDDETECTED -> ACTIVATE, FIELDLOST -> SENSE.
    reg32(NFCT_BASE, NFCT_SHORTS) = NFCT_SHORTS_FIELDDETECTED_ACTIVATE |
                                     NFCT_SHORTS_FIELDLOST_SENSE;

    // Interrupts: field events + RX/TX end + error + selected.
    const uint32_t inten = NFCT_INTEN_FIELDDETECTED | NFCT_INTEN_FIELDLOST |
                            NFCT_INTEN_RXFRAMEEND  | NFCT_INTEN_TXFRAMEEND |
                            NFCT_INTEN_ERROR        | NFCT_INTEN_SELECTED;
    reg32(NFCT_BASE, NFCT_INTENSET) = inten;
    enableNvic(NFCT_IRQ_NUMBER);
    return true;
}

void NrfNfcTag::handleCommand(uint8_t cmd, uint8_t arg) {
    // Type 2 commands - we only implement READ (0x30) since we're read-only.
    if (cmd == 0x30U) {
        // READ block N: respond with 16 bytes starting at block N (N*4).
        size_t block = arg;
        size_t offset = block * 4U;
        if (offset + 16U > TAG_MEMORY_BYTES) {
            // Out-of-range: respond with NAK 0x00 (Type 2 NAK).
            txBuffer_[0] = 0x00U;
            reg32(NFCT_BASE, NFCT_PACKETPTR) = reinterpret_cast<uint32_t>(txBuffer_);
            reg32(NFCT_BASE, NFCT_TX_AMOUNT) = 4U;   // 4-bit NAK frame
            reg32(NFCT_BASE, NFCT_TASKS_STARTTX) = 1UL;
            return;
        }
        memcpy(txBuffer_, &tagMemory_[offset], 16);
        reg32(NFCT_BASE, NFCT_PACKETPTR) = reinterpret_cast<uint32_t>(txBuffer_);
        reg32(NFCT_BASE, NFCT_TX_AMOUNT) = 16U * 8U;  // 16 bytes in bit units
        reg32(NFCT_BASE, NFCT_TASKS_STARTTX) = 1UL;
        ++readCount_;
        if (readCb_) readCb_();
    } else if (cmd == 0xA2U) {
        // WRITE - we're read-only, send NAK.
        txBuffer_[0] = 0x00U;
        reg32(NFCT_BASE, NFCT_PACKETPTR) = reinterpret_cast<uint32_t>(txBuffer_);
        reg32(NFCT_BASE, NFCT_TX_AMOUNT) = 4U;
        reg32(NFCT_BASE, NFCT_TASKS_STARTTX) = 1UL;
    } else {
        // Unknown command - go back to listening.
        reg32(NFCT_BASE, NFCT_PACKETPTR) = reinterpret_cast<uint32_t>(rxBuffer_);
        reg32(NFCT_BASE, NFCT_MAXLEN) = sizeof(rxBuffer_);
        reg32(NFCT_BASE, NFCT_TASKS_ENABLERXDATA) = 1UL;
    }
}

void NrfNfcTag::serviceIrq() {
    // FIELDDETECTED
    if (reg32(NFCT_BASE, NFCT_EVENTS_FIELDDETECTED) != 0UL) {
        reg32(NFCT_BASE, NFCT_EVENTS_FIELDDETECTED) = 0UL;
        ++fieldDetectCount_;
        if (fieldDetectCb_) fieldDetectCb_();
    }
    // FIELDLOST
    if (reg32(NFCT_BASE, NFCT_EVENTS_FIELDLOST) != 0UL) {
        reg32(NFCT_BASE, NFCT_EVENTS_FIELDLOST) = 0UL;
        ++fieldLostCount_;
        if (fieldLostCb_) fieldLostCb_();
    }
    // SELECTED - reader chose us. Arm RX for the first command.
    if (reg32(NFCT_BASE, NFCT_EVENTS_SELECTED) != 0UL) {
        reg32(NFCT_BASE, NFCT_EVENTS_SELECTED) = 0UL;
        reg32(NFCT_BASE, NFCT_PACKETPTR) = reinterpret_cast<uint32_t>(rxBuffer_);
        reg32(NFCT_BASE, NFCT_MAXLEN) = sizeof(rxBuffer_);
        reg32(NFCT_BASE, NFCT_TASKS_ENABLERXDATA) = 1UL;
    }
    // RXFRAMEEND - command from reader complete.
    if (reg32(NFCT_BASE, NFCT_EVENTS_RXFRAMEEND) != 0UL) {
        reg32(NFCT_BASE, NFCT_EVENTS_RXFRAMEEND) = 0UL;
        const uint32_t bits = reg32(NFCT_BASE, NFCT_RX_AMOUNT);
        const uint32_t bytes = bits / 8U;
        if (bytes >= 2U) {
            handleCommand(rxBuffer_[0], rxBuffer_[1]);
        } else {
            // re-arm
            reg32(NFCT_BASE, NFCT_PACKETPTR) = reinterpret_cast<uint32_t>(rxBuffer_);
            reg32(NFCT_BASE, NFCT_MAXLEN) = sizeof(rxBuffer_);
            reg32(NFCT_BASE, NFCT_TASKS_ENABLERXDATA) = 1UL;
        }
    }
    // TXFRAMEEND - our response is out. Re-arm RX for next command.
    if (reg32(NFCT_BASE, NFCT_EVENTS_TXFRAMEEND) != 0UL) {
        reg32(NFCT_BASE, NFCT_EVENTS_TXFRAMEEND) = 0UL;
        reg32(NFCT_BASE, NFCT_PACKETPTR) = reinterpret_cast<uint32_t>(rxBuffer_);
        reg32(NFCT_BASE, NFCT_MAXLEN) = sizeof(rxBuffer_);
        reg32(NFCT_BASE, NFCT_TASKS_ENABLERXDATA) = 1UL;
    }
    // Error - just clear the bit so we don't stick. NFCT will resync.
    if (reg32(NFCT_BASE, NFCT_EVENTS_ERROR) != 0UL) {
        reg32(NFCT_BASE, NFCT_EVENTS_ERROR) = 0UL;
        reg32(NFCT_BASE, NFCT_ERRORSTATUS) = 0xFFFFFFFFUL;
    }
}

extern "C" void NFCT_IRQHandler(void) {
    nrfNfcTag().serviceIrq();
}
