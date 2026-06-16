# Zigbee / IEEE 802.15.4 on ArduinoNRF

ArduinoNRF now tracks two Zigbee paths:

1. **Official onboard Zigbee firmware** for the nRF52840's own RADIO, built with
   Nordic's nRF Connect SDK Zigbee R23 add-on. This is the future-facing path.
2. **External CC2530 Zigbee / 802.15.4**, the already verified Arduino path that
   uses a UART radio module and the separate NiusZigbee library.

These paths solve different problems. The onboard path runs official Nordic
firmware on the nRF52840 and replaces the Arduino sketch. The CC2530 path keeps
the Arduino sketch running on the nRF52840 and uses an external transceiver.

## Official onboard Zigbee: nCS Zigbee R23 sidecar

This is the priority path for "nRF52840 without CC2530".

The first implementation target is a sidecar firmware flow:

```text
nRF Connect SDK + Zigbee R23 add-on -> official Zigbee firmware .hex
ArduinoNRF board metadata/tools      -> board overlay, build wrapper, flash wrapper
board1 + J-Link                      -> first hardware validation target
```

Tracked files:

- Tools: [`hardware/arduinonrf/nrf52/tools/ncs_zigbee/`](../../hardware/arduinonrf/nrf52/tools/ncs_zigbee/)
- Pins: [`pins.json`](../../hardware/arduinonrf/nrf52/tools/ncs_zigbee/pins.json)
- Planning note: `F:\Arduino\driver\arduinonrf_improve.md` in the local driver
  workspace.

### What this path means

- Uses the nRF52840's onboard IEEE 802.15.4 RADIO.
- Uses Nordic's official Zigbee R23 add-on / ZBOSS stack through nCS.
- First firmware targets are `ncp_usb`, `shell`, and `coordinator`.
- First board target is physical **board1** (`promicro_nrf52840`) because it has
  a J-Link connected.
- Bootloaders are not reflashed by default.
- Local SDK downloads and builds go in `.ncs-zigbee-work/`, which is ignored by
  Git.

### What this path does not mean yet

- It does not make `Zigbee.begin()` in a normal Arduino sketch run ZBOSS.
- It does not run NimBLE and Zigbee together in the current bare-metal Arduino
  runtime.
- It does not promise Zigbee product certification.
- It does not bundle ZBOSS binaries inside ArduinoNRF.

### Check the sidecar environment

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\build_zigbee.ps1 `
  -Board promicro_nrf52840 -Target ncp_usb -CheckOnly
```

Expected: the script prints the selected board/target, checks for `west`,
`cmake`, and Python, and creates `.ncs-zigbee-work/` if needed. In check-only
mode it does not download, build, or flash anything.

### Flash policy

The sidecar flash wrapper is intentionally narrow:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  hardware\arduinonrf\nrf52\tools\ncs_zigbee\flash_zigbee.ps1 `
  -Board promicro_nrf52840 -Hex .ncs-zigbee-work\build\promicro_nrf52840\ncp_usb\zephyr\zephyr.hex
```

It uses J-Link application flashing only. It does not run `recover`, `eraseall`,
or bootloader flashing commands.

## Existing working path: external CC2530

Use this when you want an Arduino sketch to keep running on the nRF52840 while
Zigbee / 802.15.4 traffic is handled by an external module.

- **Driver library:** **[ArduinoNRF-Zigbee](https://github.com/dunknowcoding/ArduinoNRF-Zigbee)**
  - Install it separately in the Arduino IDE.
  - Provides raw 802.15.4 send / receive / promiscuous sniff through the
    `CC2530Radio` API over `Serial1`.
  - Bundles the SDCC transceiver firmware for the module.
- **Built-in flasher:** [`libraries/CCDebugger/`](../../hardware/arduinonrf/nrf52/libraries/CCDebugger/)
  turns the nRF52840 into a TI CC2530 programmer.

Current lab state:

- board1-board5 each have a CC2530 module connected.
- The NiusZigbee path remains usable while the official onboard path is being
  added.

### Quick use

1. Install the **ArduinoNRF board package** and the **ArduinoNRF-Zigbee** library.
2. Wire **DD/DC/RST -> D8/D9/D10** for flashing and **UART -> D0/D1** for
   runtime, all at **3.3 V**.
3. Run *Examples -> ArduinoNRF-Zigbee -> CC2530_FlashFirmware* once.
4. Use *CC2530_Info / CC2530_Sniffer / CC2530_Link*.

### Notes

- 3.3 V only; the CC2530 is not 5 V tolerant.
- This external path can coexist with Arduino BLE because the nRF52840 RADIO is
  not used by the CC2530 module.
- This path is not the same thing as official onboard ZBOSS firmware.

## `libraries/Zigbee` status

[`libraries/Zigbee/`](../../hardware/arduinonrf/nrf52/libraries/Zigbee/) remains
an experimental Arduino API placeholder. Until a future phase explicitly wires a
real backend, `begin()` returns `ZIGBEE_NOT_VENDORED` and `isAvailable()` returns
`false`.

Do not use this skeleton as evidence that official nRF-native Zigbee is already
linked into Arduino sketches. The official path is the nCS sidecar flow above.
