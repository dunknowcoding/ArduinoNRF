# Compatibility — OS support & real-board status

What this core actually supports on **Windows / Linux / macOS**, and how its board definitions line up with the **real hardware on sale**. Updated after a full audit and an authoritative cross-check against the upstream `Adafruit_nRF52_Bootloader` board.h files and manufacturer sources.

> ⚠️ Only the **AliExpress ProMicro nRF52840** (verified on physical hardware) is end-to-end *tested*. The other boards have authoritative metadata derived from upstream sources but have not been re-verified on hardware in this revision — your help testing on real boards is welcome.

## 🖥️ Operating-system support

| OS | Hands-free upload | Single-cable debug | How it's done |
|---|---|---|---|
| **Windows 10/11 (x86_64)** | ✅ Verified | ✅ Verified | Native `upload.ps1` (touch + UF2 or adafruit-nrfutil), `.NET` GDB bridge |
| **Linux (Ubuntu/Debian/Fedora/Arch, x86_64 + arm64)** | ✅ Implemented (untested here) | ✅ Implemented (untested here) | `upload.py` → `adafruit-nrfutil`; `usb_gdbstub_bridge.py` |
| **macOS (Intel + Apple Silicon)** | ✅ Implemented (untested here) | ✅ Implemented (untested here) | `upload.py` (locale-wrapped) → `adafruit-nrfutil`; `usb_gdbstub_bridge.py` |

### Setup on Linux / macOS

```bash
# 1. Install adafruit-nrfutil (pulls in pyserial; brings cross-platform DFU)
pip3 install --user adafruit-nrfutil

# Ensure pip's user bin is on PATH:
#   Linux:  export PATH="$HOME/.local/bin:$PATH"
#   macOS:  export PATH="$HOME/Library/Python/3.X/bin:$PATH"

# 2. Linux only — udev rules so non-root users can touch + flash + debug
sudo cp hardware/arduinonrf/nrf52/tools/niusrobotlab/99-arduinonrf.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo usermod -a -G dialout $USER     # then log out / log back in

# 3. Use Arduino IDE or arduino-cli normally; upload + debug just work.
```

The Linux/macOS path follows the **same proven Adafruit cross-platform method** (a single `adafruit-nrfutil` recipe, IDE-driven 1200-bps touch), so it's expected to work — but the verification cycle for this revision ran on Windows only. If you exercise it on Linux/macOS, please open an issue with the result.

### Why the Windows path is a separate, larger script

`upload.ps1` exists because Windows' `usbser.sys` has clone-board quirks (same-PID app/bootloader ambiguity, stale handles after touch, queued writes replayed on next open). It also owns the Windows UF2 path so it can match a mounted drive back to the selected upload COM instead of trusting a shared volume label. On Linux/macOS the kernel CDC ACM driver is well-behaved, so the simple `adafruit-nrfutil --touch 1200 dfu serial` flow that Adafruit uses is sufficient and that's what `upload.py` does. The PowerShell features specific to Windows (concurrency mutex, bridge-yield IPC, stable-ID UF2 matching, fast-path PnP cache) are not currently replicated in the Python path.

## 🧩 Real-board compatibility matrix

Identities below were corrected against the upstream `Adafruit_nRF52_Bootloader` `board.h` files, the Seeed and pdcook Arduino cores, and `joric/nrfmicro`. See "Sources" at the bottom.

| Board (lib name) | Bootloader VID:PID | Runtime VID:PID | Bootloader | UF2 label / board-id | Notes |
|---|---|---|---|---|---|
| `promicro_nrf52840` | `0x239A:0x00B3` | `0x239A:0x00B4` | UF2 + Adafruit serial DFU | `NICENANO` / `nRF52840-nicenano` | ✅ **Verified on hardware**; identical bootloader to nice!nano (the clone ships with it). |
| `nicenano_v2` | `0x239A:0x00B3` | `0x239A:0x00B4` | UF2 + Adafruit serial DFU | `NICENANO` / `nRF52840-nicenano` | Identity matches upstream; pipeline is the same as ProMicro. |
| `supermini_nrf52840` | `0x239A:0x00B3` | `0x239A:0x00B4` | UF2 + Adafruit serial DFU | `NICENANO` | The AliExpress SuperMini ships with the nice!nano bootloader — same identity. LED on P0.15. |
| `nrfmicro_nrf52840` | `0x1209:0x5284` | `0x1209:0x5285` | Adafruit serial DFU | `NRFMICRO` / `nRF52840-nRFMicro-v0` | joric open-hardware identity. Some DIY/JLCPCB builds are flashed with nice!nano IDs instead — verify before relying on the VID:PID. |
| `xiao_nrf52840` | `0x2886:0x0044` | `0x2886:0x8044` | Adafruit serial DFU (Seeed) | `XIAO-BOOT` / `nRF52840-SeeedXiao-v1` | Seeed XIAO nRF52840. Sense variant uses `0x2886:0x0045/0x8045` (covered in the auto-detect table). |
| `pitaya_go_nrf52840` | `0x2886:0xF00E` | `0x2886:0xF00E` | Adafruit serial DFU | `PITAYAGO` / `PITAYAGO` | Makerdiary Pitaya Go. |
| `mini_nrf52840` | `auto` | `auto` | varies | varies | ⚠️ No canonical identity — auto-detect; identity depends on whichever bootloader the seller flashed. |
| `devboard_nrf52840` | `auto` | `auto` | varies | varies | ⚠️ The official Nordic nRF52840-DK uses **SEGGER J-Link OB** (VID `0x1366`), not a USB DFU bootloader — flash via SWD on the DK. AliExpress "dev boards" vary. |
| `usb_dongle_nrf52840` | `0x1915:0x521F` | n/a | **Nordic Open DFU** (CDC ACM) | (no UF2 volume) | ⚠️ **Pipeline-incompatible**: this dongle uses Nordic's Open Bootloader (not Adafruit serial DFU). Flash it with Nordic `nrfutil` or **nRF Connect for Desktop → Programmer**. This core's upload pipeline is not the right tool for the PCA10059. |
| `devboard_nrf52833` | n/a | n/a | varies | varies | ⚠️ Generic nRF52833 dev board — identity varies by seller. |

### What changed in this audit

| Board | Before | After |
|---|---|---|
| nice!nano v2 | `0x1915:0x5286` (placeholder) | `0x239A:0x00B3` + `runtime 0x00B4` ✅ |
| SuperMini | `0x1915:0x5287` (placeholder) | `0x239A:0x00B3` + `runtime 0x00B4` ✅ |
| nRFMicro | `0x1915:0x5288` + `nordic-dfu` (wrong type) | `0x1209:0x5284/0x5285` + `adafruit-dfu` ✅ |
| XIAO | `0x1915:0x5283` + empty UF2 metadata | `0x2886:0x0044/0x8044` + `XIAO-BOOT` ✅ |
| Pitaya Go | `0x1915:0x5284` + `nordic-dfu` (wrong type) | `0x2886:0xF00E` + `adafruit-dfu` + `PITAYAGO` ✅ |
| USB Dongle | `0x1915:0x5285` + `uf2` (wrong type) | `0x1915:0x521F` + `nordic-dfu` + flagged incompatible ✅ |
| Mini | `0x1915:0x5282` (placeholder) | `auto` + flagged variable |
| Generic devboard | `0x1915:0x5280` (placeholder) | `auto` + flagged variable / DK uses J-Link |

The auto-detect table in `upload.ps1` (`$script:NrfBootloaderCandidates`) was also extended with the Seeed (`0x2886:0x0044/0x8044/0x0045/0x8045`), Makerdiary (`0x2886:0xF00E`) and pid.codes nRFMicro (`0x1209:0x5284/0x5285`) entries, so `bootloader_mode=auto` also recognises them.

## 🔭 What's not (yet) covered

- **Nordic nRF52840-DK (official, with J-Link OB)**: flash via SWD using OpenOCD / J-Link / pyOCD; this core's USB hands-free path doesn't apply.
- **Nordic nRF52840 USB Dongle (PCA10059)**: use Nordic's tooling.
- **Single-cable USB-CDC GDB-stub debug** is verified on the ProMicro clone only. The Adafruit-fork clones (nice!nano / SuperMini / Pitaya / XIAO / nRFMicro) share the necessary firmware infrastructure (DebugMonitor + USB CDC), so it's expected to work, but it has not been re-verified on those boards in this revision.
- **Windows UF2 stable-ID matching** is verified with two ProMicro/nice!nano-class boards mounted at the same time. Linux/macOS still use the serial-DFU Python path in this revision.

## 📚 Sources

- Adafruit_nRF52_Bootloader board.h files (authoritative for VID:PID, UF2 label, board-id, LED pins): <https://github.com/adafruit/Adafruit_nRF52_Bootloader/tree/master/src/boards>
- Seeed XIAO Arduino core boards.txt (XIAO `0x0044`/`0x8044`): <https://github.com/Seeed-Studio/Adafruit_nRF52_Arduino/blob/master/boards.txt>
- pdcook nRFMicro-Arduino-Core (SuperMini LED P0.15, nRFMicro runtime PID): <https://github.com/pdcook/nRFMicro-Arduino-Core>
- joric nrfmicro wiki (nRFMicro identity, SuperMini ships nice!nano bootloader): <https://github.com/joric/nrfmicro/wiki/Bootloader>, <https://github.com/joric/nrfmicro/wiki/Alternatives>
- Adafruit_nRF52_Arduino platform.txt (the cross-platform upload pattern this lib's Linux/macOS recipe follows): <https://raw.githubusercontent.com/adafruit/Adafruit_nRF52_Arduino/master/platform.txt>
- Makerdiary Pitaya Go (Adafruit-fork, PITAYAGO label): <https://wiki.makerdiary.com/pitaya-go/>
- Nordic nRF52840 USB Dongle (PCA10059, Open Bootloader `0x1915:0x521F`): <https://docs.nordicsemi.com/bundle/ug_nrf52840_dongle/page/UG/nrf52840_Dongle/intro.html>
