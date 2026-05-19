# Release Notes — v0.1.3

Cosmetic-only iteration on top of [v0.1.2](RELEASE_NOTES_v0.1.2.md). Same
install URL:

```
https://raw.githubusercontent.com/dunknowcoding/ArduinoNRF/main/package_arduinonrf_index.json
```

If you already installed `0.1.0–0.1.2`, open Arduino IDE 2.x's
**Tools → Board → Boards Manager**, search for **ArduinoNRF**, and click
**Update**.

## What changed — menu labels for `promicro_nrf52840`

Bootloader-mode menu entries are now short, scannable, ordered by use
case, and each entry surfaces its FQBN key in square brackets so docs
that reference `bootloader=promicroserialnosd` (etc.) are searchable
right from Tools → Bootloader mode in the IDE.

| Old label | New label |
|---|---|
| ProMicro / clone serial DFU no SoftDevice (0x239A:0x00B3, 0x1000) | Serial DFU, no SoftDevice, app @ 0x1000 `[promicroserialnosd]` |
| ProMicro / clone serial DFU (0x239A:0x00B3, 0x26000) | Serial DFU, S140 SoftDevice, app @ 0x26000 `[promicroserial]` |
| ProMicro / clone serial DFU legacy (0x239A:0x00B3, 0x27000) | Serial DFU, S140, app @ 0x27000 (legacy) `[promicroseriallegacy]` |
| ProMicro / clone UF2 mounted drive (0x239A:0x00B3, 0x26000) | UF2 drive, S140 SoftDevice, app @ 0x26000 `[promicro]` |
| ProMicro / clone UF2 mounted drive legacy (0x239A:0x00B3, 0x27000) | UF2 drive, S140, app @ 0x27000 (legacy) `[promicrolegacy]` |
| nice!nano v2 UF2 (0x239A:0x0029) | nice!nano v2 UF2 bootloader `[nicenano]` |
| Nordic USB DFU | Nordic Open USB DFU `[nordicdfu]` |
| Seeed XIAO UF2 | Seeed XIAO UF2 bootloader `[xiaouf2]` |
| MakerDiary UF2 | MakerDiary UF2 bootloader `[makerdiaryuf2]` |
| Auto-detect (default, recommended) | Auto-detect (recommended) `[auto]` |

Other tightened menus:

- **USB descriptor menu**: `Application DFU descriptor (default)` →
  `With application DFU`; `Experimental: omit application DFU interface`
  → `Without application DFU`.
- **Build profile**: parens around `-Og -g3` flags swapped to brackets
  for visual consistency; the `usbgdbstub` profile now says explicitly
  "USB CDC GDB stub for IDE 2 Debug `[-Og -g3]`".

Nothing else changed — same firmware, same upload chain, same V1/V2/V3
behaviour, same one-click Debug wiring from v0.1.1.
