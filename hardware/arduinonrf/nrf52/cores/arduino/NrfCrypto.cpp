// NrfCrypto.cpp - implementation of the on-chip AES peripherals (ECB / CCM /
// AAR). Standalone: no Arduino.h dependency. All operations are blocking and
// poll the relevant END event; the ops finish in single-digit microseconds.

#include "NrfCrypto.h"
#include <string.h>

namespace {

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

// Spin-wait for an event register to become non-zero. Returns false on
// timeout (the spin bound is generous - these ops are microseconds).
inline bool waitEvent(uint32_t base, uint32_t evtOffset) {
    for (uint32_t spin = 0; spin < 1000000UL; ++spin) {
        if (reg32(base, evtOffset) != 0UL) return true;
    }
    return false;
}

// ---- ECB registers (base 0x4000E000) --------------------------------------
constexpr uint32_t ECB_BASE            = 0x4000E000UL;
constexpr uint32_t ECB_TASKS_STARTECB  = 0x000UL;
constexpr uint32_t ECB_TASKS_STOPECB   = 0x004UL;
constexpr uint32_t ECB_EVENTS_ENDECB   = 0x100UL;
constexpr uint32_t ECB_EVENTS_ERRORECB = 0x104UL;
constexpr uint32_t ECB_ECBDATAPTR      = 0x504UL;

// The 48-byte ECB data block the hardware reads via EasyDMA. Must live in RAM
// (it does - .bss) and be 32-bit aligned. Layout fixed by the PS:
//   [ 0..15] KEY        (MSB first)
//   [16..31] CLEARTEXT  (MSB first)
//   [32..47] CIPHERTEXT (output, MSB first)
struct __attribute__((aligned(4))) EcbData {
    uint8_t key[16];
    uint8_t cleartext[16];
    uint8_t ciphertext[16];
};
EcbData g_ecbData;

// ---- CCM registers (base 0x4000F000) ---------------------------------------
constexpr uint32_t CCM_BASE             = 0x4000F000UL;
constexpr uint32_t CCM_TASKS_KSGEN      = 0x000UL;
constexpr uint32_t CCM_TASKS_CRYPT      = 0x004UL;
constexpr uint32_t CCM_TASKS_STOP       = 0x008UL;
constexpr uint32_t CCM_EVENTS_ENDKSGEN  = 0x100UL;
constexpr uint32_t CCM_EVENTS_ENDCRYPT  = 0x104UL;
constexpr uint32_t CCM_EVENTS_ERROR     = 0x108UL;
constexpr uint32_t CCM_SHORTS           = 0x200UL;
constexpr uint32_t CCM_MICSTATUS        = 0x400UL;
constexpr uint32_t CCM_ENABLE           = 0x500UL;
constexpr uint32_t CCM_MODE             = 0x504UL;
constexpr uint32_t CCM_CNFPTR           = 0x508UL;
constexpr uint32_t CCM_INPTR            = 0x50CUL;
constexpr uint32_t CCM_OUTPTR           = 0x510UL;
constexpr uint32_t CCM_SCRATCHPTR       = 0x514UL;
constexpr uint32_t CCM_MAXPACKETSIZE    = 0x518UL;

constexpr uint32_t CCM_SHORTS_ENDKSGEN_CRYPT = 1UL << 0;
constexpr uint32_t CCM_ENABLE_ENABLED        = 2UL;

// ---- AAR registers (base 0x4000F000, shared with CCM) ----------------------
constexpr uint32_t AAR_BASE             = 0x4000F000UL;
constexpr uint32_t AAR_TASKS_START      = 0x000UL;
constexpr uint32_t AAR_TASKS_STOP       = 0x008UL;
constexpr uint32_t AAR_EVENTS_END       = 0x100UL;
constexpr uint32_t AAR_EVENTS_RESOLVED  = 0x104UL;
constexpr uint32_t AAR_EVENTS_NOTRESOLVED = 0x108UL;
constexpr uint32_t AAR_STATUS           = 0x400UL;
constexpr uint32_t AAR_ENABLE           = 0x500UL;
constexpr uint32_t AAR_NIRK             = 0x504UL;
constexpr uint32_t AAR_IRKPTR           = 0x508UL;
constexpr uint32_t AAR_ADDRPTR          = 0x510UL;
constexpr uint32_t AAR_SCRATCHPTR       = 0x514UL;

constexpr uint32_t AAR_ENABLE_ENABLED   = 3UL;

// AAR needs >=3 bytes of scratch RAM it can write to during resolution.
uint8_t g_aarScratch[16] __attribute__((aligned(4)));
// Caller-owned address is copied here so ADDRPTR always points at RAM.
uint8_t g_aarAddr[6] __attribute__((aligned(4)));

}  // namespace

// ============================================================================
// NrfEcb
// ============================================================================

bool NrfEcb::encrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    memcpy(g_ecbData.key, key, 16);
    memcpy(g_ecbData.cleartext, in, 16);

    reg32(ECB_BASE, ECB_ECBDATAPTR) = reinterpret_cast<uint32_t>(&g_ecbData);
    reg32(ECB_BASE, ECB_EVENTS_ENDECB)   = 0UL;
    reg32(ECB_BASE, ECB_EVENTS_ERRORECB) = 0UL;
    reg32(ECB_BASE, ECB_TASKS_STARTECB)  = 1UL;

    // Either ENDECB or ERRORECB will fire.
    for (uint32_t spin = 0; spin < 1000000UL; ++spin) {
        if (reg32(ECB_BASE, ECB_EVENTS_ERRORECB) != 0UL) {
            reg32(ECB_BASE, ECB_TASKS_STOPECB) = 1UL;
            return false;
        }
        if (reg32(ECB_BASE, ECB_EVENTS_ENDECB) != 0UL) {
            memcpy(out, g_ecbData.ciphertext, 16);
            return true;
        }
    }
    return false;
}

bool NrfEcb::selfTest() {
    static const uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };
    static const uint8_t plain[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    };
    static const uint8_t expected[16] = {
        0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
        0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a,
    };
    uint8_t got[16];
    if (!encrypt(key, plain, got)) return false;
    return memcmp(got, expected, 16) == 0;
}

// ============================================================================
// NrfCcm
// ============================================================================

bool NrfCcm::crypt(const uint8_t *inPdu, uint8_t *outPdu) {
    reg32(CCM_BASE, CCM_INPTR)  = reinterpret_cast<uint32_t>(inPdu);
    reg32(CCM_BASE, CCM_OUTPTR) = reinterpret_cast<uint32_t>(outPdu);

    reg32(CCM_BASE, CCM_EVENTS_ENDKSGEN) = 0UL;
    reg32(CCM_BASE, CCM_EVENTS_ENDCRYPT) = 0UL;
    reg32(CCM_BASE, CCM_EVENTS_ERROR)    = 0UL;

    // KSGEN -> (shortcut) -> CRYPT, then wait for ENDCRYPT.
    reg32(CCM_BASE, CCM_TASKS_KSGEN) = 1UL;

    for (uint32_t spin = 0; spin < 1000000UL; ++spin) {
        if (reg32(CCM_BASE, CCM_EVENTS_ERROR) != 0UL) {
            reg32(CCM_BASE, CCM_TASKS_STOP) = 1UL;
            return false;
        }
        if (reg32(CCM_BASE, CCM_EVENTS_ENDCRYPT) != 0UL) {
            return true;
        }
    }
    return false;
}

void NrfCcm::begin(Mode mode, DataRate rate, Config *cnf,
                   uint8_t *scratch, size_t scratchLen) {
    (void)scratchLen;
    // MODE: bit0 = enc/dec, bits16..17 = data rate, bit24 = extended length.
    reg32(CCM_BASE, CCM_MODE) =
        (static_cast<uint32_t>(mode) << 0) |
        (static_cast<uint32_t>(rate) << 16) |
        (1UL << 24);                       // extended length (BLE DLE up to 251)
    reg32(CCM_BASE, CCM_MAXPACKETSIZE) = 251UL;

    reg32(CCM_BASE, CCM_CNFPTR)     = reinterpret_cast<uint32_t>(cnf);
    reg32(CCM_BASE, CCM_SCRATCHPTR) = reinterpret_cast<uint32_t>(scratch);

    // Chain KSGEN -> CRYPT automatically so crypt() only triggers KSGEN.
    reg32(CCM_BASE, CCM_SHORTS) = CCM_SHORTS_ENDKSGEN_CRYPT;
    reg32(CCM_BASE, CCM_ENABLE) = CCM_ENABLE_ENABLED;
}

void NrfCcm::end() {
    reg32(CCM_BASE, CCM_TASKS_STOP) = 1UL;
    reg32(CCM_BASE, CCM_ENABLE)     = 0UL;
    reg32(CCM_BASE, CCM_SHORTS)     = 0UL;
}

bool NrfCcm::micOk() {
    return (reg32(CCM_BASE, CCM_MICSTATUS) & 1UL) != 0UL;
}

// ============================================================================
// NrfAar
// ============================================================================

void NrfAar::begin(const uint8_t *irks, uint8_t nIrks) {
    if (nIrks > MAX_IRKS) nIrks = MAX_IRKS;
    reg32(AAR_BASE, AAR_NIRK)       = nIrks;
    reg32(AAR_BASE, AAR_IRKPTR)     = reinterpret_cast<uint32_t>(irks);
    reg32(AAR_BASE, AAR_SCRATCHPTR) = reinterpret_cast<uint32_t>(g_aarScratch);
    reg32(AAR_BASE, AAR_ADDRPTR)    = reinterpret_cast<uint32_t>(g_aarAddr);
    reg32(AAR_BASE, AAR_ENABLE)     = AAR_ENABLE_ENABLED;
}

void NrfAar::end() {
    reg32(AAR_BASE, AAR_TASKS_STOP) = 1UL;
    reg32(AAR_BASE, AAR_ENABLE)     = 0UL;
}

int NrfAar::resolve(const uint8_t address[6]) {
    memcpy(g_aarAddr, address, 6);
    reg32(AAR_BASE, AAR_ADDRPTR) = reinterpret_cast<uint32_t>(g_aarAddr);

    reg32(AAR_BASE, AAR_EVENTS_END)          = 0UL;
    reg32(AAR_BASE, AAR_EVENTS_RESOLVED)     = 0UL;
    reg32(AAR_BASE, AAR_EVENTS_NOTRESOLVED)  = 0UL;
    reg32(AAR_BASE, AAR_TASKS_START)         = 1UL;

    if (!waitEvent(AAR_BASE, AAR_EVENTS_END)) return -1;

    if (reg32(AAR_BASE, AAR_EVENTS_RESOLVED) != 0UL) {
        return static_cast<int>(reg32(AAR_BASE, AAR_STATUS));
    }
    return -1;   // NOTRESOLVED or no match
}
