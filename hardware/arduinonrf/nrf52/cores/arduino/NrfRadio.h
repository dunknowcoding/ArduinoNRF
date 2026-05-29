// NrfRadio.h - bottom-level driver for the 2.4 GHz RADIO in *proprietary*
// (non-BLE) mode. This is the raw packet radio: two nRF5x chips (or an nRF5x
// and a compatibly-configured nRF24L01) exchange fixed/variable-length packets
// with hardware CRC. It's the foundation Enhanced ShockBurst (auto-ack /
// retransmit) builds on, and it's the way to do low-latency wireless that
// isn't BLE / 802.15.4.
//
// Relationship to the other RADIO users:
//   * NrfBleRadio (NrfBleHw.h) drives the same RADIO peripheral for BLE
//     advertising. NrfRadio drives it in proprietary mode. They are mutually
//     exclusive - the RADIO is a single resource. begin() here takes it over;
//     don't run a BLE advertiser at the same time.
//   * On the SoftDevice build profiles the SoftDevice owns the RADIO. Use
//     this driver only on the no-SoftDevice path (promicroserialnosd), which
//     is the verified target for this core anyway.
//
// Mode targeted: nRF 1/2 Mbit proprietary PHY with a 5-byte address (4 base +
// 1 prefix), 16-bit CRC, little-endian, no whitening. Two boards running this
// driver with matching channel + address + data-rate will talk to each other.
// nRF24L01 interop is possible but its CRC/address framing differs in detail;
// the verified-intended peer is another nRF5x.
//
// API is blocking. RADIO ops are tens of microseconds; receive() takes an
// explicit millisecond timeout backed by the DWT cycle counter.

#pragma once

#include <stdint.h>

class NrfRadio {
public:
    enum DataRate : uint8_t {
        RATE_1MBIT = 0,
        RATE_2MBIT = 1,
    };

    // Hardware supports up to 255-byte payloads; we cap at 32 to match the
    // common ShockBurst convention and keep the static packet buffer small.
    static constexpr uint8_t MAX_PAYLOAD = 32U;

    // Bring up the RADIO in proprietary mode. channel 0..100 maps to
    // (2400 + channel) MHz. txPowerDbm accepts the documented codes
    // (+8,+7,+6,+5,+4,+3,+2,0,-4,-8,-12,-16,-20,-30,-40); other values are
    // rounded to the nearest legal code. Returns false on bad channel.
    static bool begin(uint8_t channel, DataRate rate = RATE_2MBIT, int8_t txPowerDbm = 0);
    static void end();

    static void setChannel(uint8_t channel);     // 0..100
    static void setTxPower(int8_t dbm);

    // 5-byte on-air address (4 base bytes + 1 prefix byte). Both ends must use
    // the same address to communicate. Defaults to {0xE7,0xE7,0xE7,0xE7,0xE7}.
    static void setAddress(const uint8_t addr[5]);

    // Transmit one payload (1..MAX_PAYLOAD bytes), blocking until the packet
    // has left the antenna. Returns false on bad length / hardware timeout.
    static bool send(const uint8_t *payload, uint8_t len);

    // Wait up to timeoutMs for a valid (CRC-OK) packet. On success copies up
    // to maxLen payload bytes into out and returns the payload length (>0).
    // Returns 0 on timeout or CRC error.
    static uint8_t receive(uint8_t *out, uint8_t maxLen, uint32_t timeoutMs);

    // RSSI (in dBm, negative) sampled during the last receive(). 0 if none.
    static int8_t lastRssiDbm();
};
