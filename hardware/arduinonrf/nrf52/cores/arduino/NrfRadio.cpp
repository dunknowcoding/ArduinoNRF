// NrfRadio.cpp - proprietary 2.4 GHz packet radio. Standalone (no Arduino.h).
// Uses the DWT cycle counter for the receive() timeout so it needs no timer
// peripheral.

#include "NrfRadio.h"
#include "NrfClock.h"
#include <string.h>

namespace {

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

// ---- RADIO registers (base 0x40001000) -------------------------------------
constexpr uint32_t RADIO_BASE            = 0x40001000UL;
constexpr uint32_t RADIO_TASKS_TXEN      = 0x000UL;
constexpr uint32_t RADIO_TASKS_RXEN      = 0x004UL;
constexpr uint32_t RADIO_TASKS_START     = 0x008UL;
constexpr uint32_t RADIO_TASKS_STOP      = 0x00CUL;
constexpr uint32_t RADIO_TASKS_DISABLE   = 0x010UL;
constexpr uint32_t RADIO_TASKS_RSSISTART = 0x018UL;
constexpr uint32_t RADIO_EVENTS_READY    = 0x100UL;
constexpr uint32_t RADIO_EVENTS_END      = 0x10CUL;
constexpr uint32_t RADIO_EVENTS_DISABLED = 0x110UL;
constexpr uint32_t RADIO_EVENTS_CRCOK    = 0x130UL;
constexpr uint32_t RADIO_EVENTS_CRCERROR = 0x134UL;
constexpr uint32_t RADIO_SHORTS          = 0x200UL;
constexpr uint32_t RADIO_PACKETPTR       = 0x504UL;
constexpr uint32_t RADIO_FREQUENCY       = 0x508UL;
constexpr uint32_t RADIO_TXPOWER         = 0x50CUL;
constexpr uint32_t RADIO_MODE            = 0x510UL;
constexpr uint32_t RADIO_PCNF0           = 0x514UL;
constexpr uint32_t RADIO_PCNF1           = 0x518UL;
constexpr uint32_t RADIO_BASE0           = 0x51CUL;
constexpr uint32_t RADIO_PREFIX0         = 0x524UL;
constexpr uint32_t RADIO_TXADDRESS       = 0x52CUL;
constexpr uint32_t RADIO_RXADDRESSES     = 0x530UL;
constexpr uint32_t RADIO_CRCCNF          = 0x534UL;
constexpr uint32_t RADIO_CRCPOLY         = 0x538UL;
constexpr uint32_t RADIO_CRCINIT         = 0x53CUL;
constexpr uint32_t RADIO_RSSISAMPLE      = 0x548UL;
constexpr uint32_t RADIO_STATE           = 0x550UL;
constexpr uint32_t RADIO_MODECNF0        = 0x650UL;
constexpr uint32_t RADIO_POWER           = 0xFFCUL;

// SHORTS bits
constexpr uint32_t SHORT_READY_START   = 1UL << 0;
constexpr uint32_t SHORT_END_DISABLE   = 1UL << 1;
constexpr uint32_t SHORT_ADDRESS_RSSISTART = 1UL << 4;

// MODE values (proprietary)
constexpr uint32_t MODE_NRF_1MBIT = 0UL;
constexpr uint32_t MODE_NRF_2MBIT = 1UL;

// ---- DWT cycle counter (for receive timeout) -------------------------------
constexpr uint32_t DEMCR      = 0xE000EDFCUL;
constexpr uint32_t DWT_CTRL   = 0xE0001000UL;
constexpr uint32_t DWT_CYCCNT = 0xE0001004UL;
constexpr uint32_t DEMCR_TRCENA   = 1UL << 24;
constexpr uint32_t DWT_CYCCNTENA  = 1UL << 0;
constexpr uint32_t CYCLES_PER_MS  = 64000UL;   // 64 MHz application core

inline void dwtInit() {
    *reinterpret_cast<volatile uint32_t *>(DEMCR) |= DEMCR_TRCENA;
    *reinterpret_cast<volatile uint32_t *>(DWT_CTRL) |= DWT_CYCCNTENA;
}
inline uint32_t cycnt() { return *reinterpret_cast<volatile uint32_t *>(DWT_CYCCNT); }

// Packet RAM: byte 0 = LENGTH (LFLEN=8), bytes 1.. = payload. Must be in RAM
// and stay valid across the DMA - it's a file-scope static.
uint8_t g_packet[1U + NrfRadio::MAX_PAYLOAD] __attribute__((aligned(4)));

uint8_t g_address[5] = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 };
int8_t  g_lastRssi = 0;
bool    g_started = false;

void applyAddress() {
    // BASE0 holds the 4 base bytes; PREFIX0.AP0 holds the prefix byte. The
    // base is loaded MSB-aligned: BASE0 = b0<<24 | b1<<16 | b2<<8 | b3.
    reg32(RADIO_BASE, RADIO_BASE0) =
        (static_cast<uint32_t>(g_address[0]) << 24) |
        (static_cast<uint32_t>(g_address[1]) << 16) |
        (static_cast<uint32_t>(g_address[2]) << 8)  |
        (static_cast<uint32_t>(g_address[3]) << 0);
    reg32(RADIO_BASE, RADIO_PREFIX0) = g_address[4];   // AP0 = prefix
    reg32(RADIO_BASE, RADIO_TXADDRESS)   = 0UL;        // transmit on logical 0
    reg32(RADIO_BASE, RADIO_RXADDRESSES) = 0x1UL;      // enable RX pipe 0
}

void disableRadio() {
    reg32(RADIO_BASE, RADIO_EVENTS_DISABLED) = 0UL;
    reg32(RADIO_BASE, RADIO_TASKS_DISABLE)   = 1UL;
    for (uint32_t spin = 0; spin < 100000UL; ++spin) {
        if (reg32(RADIO_BASE, RADIO_EVENTS_DISABLED) != 0UL) break;
    }
}

}  // namespace

bool NrfRadio::begin(uint8_t channel, DataRate rate, int8_t txPowerDbm) {
    if (channel > 100U) return false;

    // RADIO needs the high-frequency crystal oscillator running for accurate
    // RF; start HFXO via the clock driver.
    nrfStartHfclk();
    dwtInit();

    // Power the RADIO on. Without this, if anything previously powered it down
    // (e.g. NimBLE shutting its controller down writes RADIO_POWER=0), every
    // config write below is ignored and the radio is silently dead.
    reg32(RADIO_BASE, RADIO_POWER) = 1UL;
    // Default (non-fast) ramp-up, like the verified BLE path. Leaving MODECNF0
    // at its reset value risks a slower/legacy ramp on some revisions.
    reg32(RADIO_BASE, RADIO_MODECNF0) = 0UL;

    reg32(RADIO_BASE, RADIO_MODE) =
        (rate == RATE_2MBIT) ? MODE_NRF_2MBIT : MODE_NRF_1MBIT;

    // PCNF0: LFLEN=8 (1-byte length field), S0LEN=0, S1LEN=0.
    reg32(RADIO_BASE, RADIO_PCNF0) = (8UL << 0);
    // PCNF1: MAXLEN=MAX_PAYLOAD, STATLEN=0, BALEN=4 (5-byte addr), little-
    // endian (bit24=0), whitening off (bit25=0).
    reg32(RADIO_BASE, RADIO_PCNF1) =
        (static_cast<uint32_t>(MAX_PAYLOAD) << 0) |
        (4UL << 16);

    // 16-bit CRC (CCITT) over address + payload, init 0xFFFF.
    reg32(RADIO_BASE, RADIO_CRCCNF)  = 2UL;            // LEN=2, SKIPADDR=0 (include)
    reg32(RADIO_BASE, RADIO_CRCPOLY) = 0x11021UL;
    reg32(RADIO_BASE, RADIO_CRCINIT) = 0xFFFFUL;

    applyAddress();
    setChannel(channel);
    setTxPower(txPowerDbm);

    reg32(RADIO_BASE, RADIO_PACKETPTR) = reinterpret_cast<uint32_t>(g_packet);
    g_started = true;
    return true;
}

void NrfRadio::end() {
    disableRadio();
    // Power the RADIO down so a later NimBLE (or re-begin) starts clean.
    reg32(RADIO_BASE, RADIO_POWER) = 0UL;
    g_started = false;
}

void NrfRadio::setChannel(uint8_t channel) {
    if (channel > 100U) channel = 100U;
    reg32(RADIO_BASE, RADIO_FREQUENCY) = channel;   // 2400 + channel MHz
}

void NrfRadio::setTxPower(int8_t dbm) {
    // Snap to the nearest legal TXPOWER code. The register takes the signed
    // dBm value directly as a byte for the supported levels.
    static const int8_t levels[] = { 8, 7, 6, 5, 4, 3, 2, 0, -4, -8, -12, -16, -20, -30, -40 };
    int8_t best = 0;
    int    bestErr = 127;
    for (int8_t lv : levels) {
        int err = (dbm > lv) ? (dbm - lv) : (lv - dbm);
        if (err < bestErr) { bestErr = err; best = lv; }
    }
    reg32(RADIO_BASE, RADIO_TXPOWER) = static_cast<uint32_t>(static_cast<uint8_t>(best));
}

void NrfRadio::setAddress(const uint8_t addr[5]) {
    memcpy(g_address, addr, 5);
    applyAddress();
}

bool NrfRadio::send(const uint8_t *payload, uint8_t len) {
    if (!g_started || payload == nullptr || len == 0U || len > MAX_PAYLOAD) {
        return false;
    }
    g_packet[0] = len;
    memcpy(&g_packet[1], payload, len);
    reg32(RADIO_BASE, RADIO_PACKETPTR) = reinterpret_cast<uint32_t>(g_packet);

    disableRadio();   // ensure we start from DISABLED
    // READY->START and END->DISABLE shortcuts: TXEN then wait DISABLED.
    reg32(RADIO_BASE, RADIO_SHORTS) = SHORT_READY_START | SHORT_END_DISABLE;
    reg32(RADIO_BASE, RADIO_EVENTS_END)      = 0UL;
    reg32(RADIO_BASE, RADIO_EVENTS_DISABLED) = 0UL;
    reg32(RADIO_BASE, RADIO_TASKS_TXEN)      = 1UL;

    for (uint32_t spin = 0; spin < 2000000UL; ++spin) {
        if (reg32(RADIO_BASE, RADIO_EVENTS_DISABLED) != 0UL) {
            reg32(RADIO_BASE, RADIO_SHORTS) = 0UL;
            return true;
        }
    }
    reg32(RADIO_BASE, RADIO_SHORTS) = 0UL;
    return false;
}

uint8_t NrfRadio::receive(uint8_t *out, uint8_t maxLen, uint32_t timeoutMs) {
    if (!g_started || out == nullptr) return 0U;

    reg32(RADIO_BASE, RADIO_PACKETPTR) = reinterpret_cast<uint32_t>(g_packet);
    disableRadio();

    // READY->START so reception begins automatically; ADDRESS->RSSISTART so an
    // RSSI sample is captured for the incoming packet. Leave the radio in RX
    // (no END_DISABLE) so we can read events after END.
    reg32(RADIO_BASE, RADIO_SHORTS) = SHORT_READY_START | SHORT_ADDRESS_RSSISTART;
    reg32(RADIO_BASE, RADIO_EVENTS_END)      = 0UL;
    reg32(RADIO_BASE, RADIO_EVENTS_CRCOK)    = 0UL;
    reg32(RADIO_BASE, RADIO_EVENTS_CRCERROR) = 0UL;
    reg32(RADIO_BASE, RADIO_TASKS_RXEN)      = 1UL;

    const uint32_t start = cycnt();
    const uint32_t budget = timeoutMs * CYCLES_PER_MS;
    uint8_t result = 0U;

    while ((cycnt() - start) < budget) {
        if (reg32(RADIO_BASE, RADIO_EVENTS_END) != 0UL) {
            reg32(RADIO_BASE, RADIO_EVENTS_END) = 0UL;
            if (reg32(RADIO_BASE, RADIO_EVENTS_CRCOK) != 0UL) {
                uint8_t len = g_packet[0];
                if (len > MAX_PAYLOAD) len = MAX_PAYLOAD;
                uint8_t n = (len < maxLen) ? len : maxLen;
                memcpy(out, &g_packet[1], n);
                g_lastRssi = -static_cast<int8_t>(reg32(RADIO_BASE, RADIO_RSSISAMPLE) & 0x7FUL);
                result = n;
            }
            break;   // CRC error or done -> stop this receive
        }
    }

    reg32(RADIO_BASE, RADIO_SHORTS) = 0UL;
    disableRadio();
    return result;
}

int8_t NrfRadio::lastRssiDbm() { return g_lastRssi; }
