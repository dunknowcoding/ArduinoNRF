# BLE and WiFi Boundaries

Date: 2026-05-04

This package currently ships an advertising-only self-hosted BLE facade. It does not ship a SoftDevice, Bluefruit compatibility layer, or a connection-oriented GATT implementation.

## BLE truth in the current package

- `BLE.begin()` only succeeds when the active board metadata declares a low-frequency clock source.
- `BLE.gattServerSupported()` is `false`.
- `BLE.connectionsSupported()` is `false`.
- `BLE.notificationsSupported()` is `false`.
- `BLE.otaDfuSupported()` is `false`.
- `BLE.selfHostedStack()` is `true`.
- `BLE.bluefruitBacked()` is `false`.

## WiFi truth in the current package

- No packaged board exposes a full Arduino WiFi stack.
- Boards that carry a WiFi coprocessor are still modeled as hardware facts only.
- There is no bundled TCP/IP, socket, scan, join, or TLS implementation backed by onboard WiFi hardware.
