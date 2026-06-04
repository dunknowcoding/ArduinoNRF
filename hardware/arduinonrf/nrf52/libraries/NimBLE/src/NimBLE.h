// NimBLE.h - Arduino API surface for the planned NimBLE (Apache Mynewt)
// integration on the nRF52840 RADIO. See
// docs/platform/NIMBLE_INTEGRATION_PLAN.md for the milestone schedule.
//
// SCOPE
//   This is an M1 bring-up slice. The actual BLE host + controller still
//   requires the NimBLE source tree vendored under vendor/ (see
//   vendor/README.md). Today begin() initializes the bare-metal porting
//   layer, requests HFCLK, and exposes the chip BLE address. The public
//   API can currently bridge advertising only into the repository's
//   in-tree advertising facade; that backend still lacks a proven over-
//   the-air controller. Real connection / GATT / security support still
//   lands with milestones M2..M5.

#pragma once

#include <stdint.h>
#include <stddef.h>

class NimBLE {
public:
    enum Status : int8_t {
        NIMBLE_OK            =  0,
        NIMBLE_NOT_VENDORED  = -1,   // NimBLE source not vendored
        NIMBLE_NOT_STARTED   = -2,
        NIMBLE_BAD_PARAM     = -3,
        NIMBLE_INTERNAL      = -4,
    };

    // -- Bring-up ----------------------------------------------------------

    // Initialize the controller + host. `deviceName` is the local name
    // advertised in scan responses (max 30 chars). Returns NIMBLE_OK on
    // real hardware after a successful BLE stack init.
    static Status begin(const char *deviceName);

    static void end();
    static bool isAvailable();
    static void poll();

    // -- Advertising (M2 milestone) ----------------------------------------

    // Default advertising payload: device name + a placeholder service UUID.
    // The full GAP / GATT service-builder API lands with M3.
    static Status startAdvertising();
    static Status stopAdvertising();

    // -- Connection state (M3 milestone) -----------------------------------

    static bool isConnected();
    static int  connectionCount();

    // -- Data transfer over the Nordic UART Service (NUS) -------------------
    // The board advertises a "BLE UART": two characteristics that let a phone
    // or PC app (e.g. nRF Connect, the bleak Python library) exchange raw
    // bytes with the sketch. This is the simplest way to send/receive data
    // over BLE - it behaves like a wireless Serial port.
    //
    //   TX (board -> central): write() sends a notification.
    //   RX (central -> board): onReceive() fires when the central writes.

    // Largest payload a single write() can send. Limited by the negotiated
    // ATT MTU; 244 is the most a 247-byte DLE MTU allows.
    static const size_t MAX_PACKET = 244;

    // Send up to MAX_PACKET bytes to the connected central as a TX
    // notification. Returns the number of bytes sent, or 0 if no central is
    // connected, it has not enabled notifications, or the stack is busy.
    static size_t write(const uint8_t *data, size_t length);
    static size_t write(const char *text);   // convenience for C strings

    // Callback type for received data. It runs from poll() context (not an
    // ISR), so Serial writes are fine - but keep it short and never block, or
    // the BLE link stalls.
    typedef void (*ReceiveCallback)(const uint8_t *data, size_t length);

    // Register the function to call when the central writes to RX. Passing
    // nullptr (the default) keeps the built-in behaviour, where received
    // bytes are echoed straight back to the central on TX.
    static void onReceive(ReceiveCallback callback);

    // -- Diagnostics -------------------------------------------------------

    // Public BLE address (6 bytes). On nRF52840 this is derived from
    // FICR.DEVICEADDR (factory-unique). Filled with 0xFF if NimBLE isn't
    // initialized.
    static void getPublicAddress(uint8_t addr[6]);
};
