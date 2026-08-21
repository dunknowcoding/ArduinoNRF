#pragma once

#include <stddef.h>
#include <stdint.h>

namespace nrf_usbd_detail {

struct ControlOutPacketProgress {
    size_t received;
    bool accepted;
    bool complete;
};

constexpr bool ep0DataDoneMayCommit(bool newerSetupPending) {
    // A new SETUP token aborts the older control transfer. Because firmware
    // must arm the new data stage after reading SETUP, a simultaneously latched
    // EP0DATADONE belongs to the aborted request.
    return !newerSetupPending;
}

constexpr bool pluggableControlOutPayloadSupported(uint16_t length, size_t capacity) {
    return length > 0U && static_cast<size_t>(length) <= capacity;
}

constexpr size_t nextControlOutPacketSize(size_t expected, size_t received,
                                          size_t maxPacket) {
    if (received >= expected || maxPacket == 0U) {
        return 0U;
    }
    const size_t remaining = expected - received;
    return remaining > maxPacket ? maxPacket : remaining;
}

constexpr ControlOutPacketProgress acceptControlOutPacket(size_t expected,
                                                          size_t received,
                                                          size_t packetLength,
                                                          size_t maxPacket) {
    const size_t expectedPacket =
        nextControlOutPacketSize(expected, received, maxPacket);
    if (expectedPacket == 0U || packetLength != expectedPacket) {
        return {received, false, false};
    }
    const size_t nextReceived = received + packetLength;
    return {nextReceived, true, nextReceived == expected};
}

}  // namespace nrf_usbd_detail
