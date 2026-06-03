# BLE and WiFi Boundaries

Date: 2026-05-04

This package ships **two** BLE surfaces. The full connection-oriented stack is
**NimBLE** (`libraries/NimBLE/`): a vendored Apache Mynewt host + link-layer
controller providing advertising, connections, MTU exchange, and complete GATT
(services / characteristics / descriptors / notifications), verified on hardware
against Windows, Android, and board-to-board. Separately, the legacy self-hosted
`BLE` facade documented below remains **advertising-only**. The package does
**not** ship a SoftDevice or a Bluefruit compatibility layer — use NimBLE, not
the `BLE` facade, for connections and GATT.

## Legacy `BLE` facade truth (advertising-only; superseded by NimBLE)

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
