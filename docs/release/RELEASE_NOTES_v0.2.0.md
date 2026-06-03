# ArduinoNRF v0.2.0

Two headline changes: **USB-CDC enumeration is now interrupt-driven** (immune to
whatever your sketch's `loop()` does), and **BLE works for real** — the vendored
NimBLE host + controller now does full GATT discovery, verified on hardware
against Windows, Android, and board-to-board.

## 🔵 BLE (NimBLE) — connection-oriented GATT works

`libraries/NimBLE/` (Apache Mynewt NimBLE host + link-layer controller on a
bare-metal cooperative port) is now a working BLE peripheral:

* Advertising, connection, **MTU exchange** (256 B), and **full GATT discovery**
  — services, characteristics, and descriptors.
* Verified on hardware from **Windows** (bleak / WinRT), **Android**
  (nRF Connect), and **board-to-board** (a second board as GATT central).
* Ships standard **GAP (0x1800)** + **GATT (0x1801)** services plus a **Nordic
  UART** service (notify / write). User `Serial` keeps working concurrently.
* Sketch model is just `NimBLE::begin(name); NimBLE::startAdvertising();` then
  `NimBLE::poll()` in `loop()`. Start from `examples/NimBLESmoke`.

### Root-cause fix behind it

GATT discovery used to stall (Windows aborted ~30 s after the first response;
board-to-board hung at MTU). Root cause, pinned with an SWD hardware watchpoint:
in this combined host+controller build the host allocated transmit mbufs
**without reserving the controller's `ble_mbuf_hdr`** (the allocators gated on
`MYNEWT_VAL(BLE_CONTROLLER)`, which is undefined here — the controller is
selected via `NIMBLE_CFG_CONTROLLER`). The header therefore overlapped the
packet data, and writing `txinfo.pyld_len` clobbered the **on-air L2CAP length
byte**, so every peer waited forever for non-existent continuation bytes. The
allocators now reserve the header whenever the local controller is present.

## 🔌 USB-CDC enumeration is now interrupt-driven

The `promicroserialnosd` option previously serviced USB only by polling between
`loop()` iterations, so a sketch that never returned from `loop()` (e.g. a
`for(;;)`) or blocked in `Serial` could drop the COM port. USB is now serviced
in the USBD ISR:

* Enumeration is **immune to user code** — both the **service/maintenance** CDC
  and the **user** CDC stay enumerated regardless of `loop()` behavior.
* A board running a non-yielding sketch is **still re-uploadable over USB** (the
  1200-bps DFU touch is serviced in the ISR too) — no manual reset needed.
* **No conflict with the USB GDB stub**: both CDC ports stay enumerated while the
  CPU is halted at a breakpoint, and a board left halted in a debug session is
  still uploadable via the normal `arduino-cli upload` path.
* USB runs at a low interrupt priority so the BLE radio/timer chain always wins
  arbitration; foreground ↔ ISR access is serialized with a small critical
  section.

## 🧹 Repo cleanup

* Pruned outdated / redundant / low-information docs and folded the thin
  per-board stubs into the board index; refreshed the BLE status across the
  docs (NimBLE is no longer described as advertising-only).
* `.gitignore` now covers BLE/WinRT debug scratch, host-side test drivers, and
  debug-instrumented example sketches.

## Verified on hardware (AliExpress ProMicro nRF52840 clone)

* BLE: full GATT discovery from Windows (bleak), Android (nRF Connect), and a
  second board acting as central; ~2 s to complete.
* USB: COM ports survive a `for(;;)` sketch; re-upload over USB with no reset;
  uploadable even while halted in the GDB stub.
* Hands-free upload and single-cable GDB-stub debug continue to work.

## Upgrade notes

No API breakage from 0.1.0. The USB change is internal to the core and requires
no sketch changes. To use BLE, include `<NimBLE.h>` and pump `NimBLE::poll()` —
see `examples/NimBLESmoke`.
