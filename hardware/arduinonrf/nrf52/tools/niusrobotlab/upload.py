#!/usr/bin/env python3
"""Cross-platform serial-DFU upload for the ArduinoNRF Adafruit-fork bootloader.

Mirrors the proven Adafruit nRF52 BSP pattern (adafruit-nrfutil dfu genpkg + dfu
serial), used on Linux and macOS where the Windows-specific upload.ps1 is not
available. The Arduino IDE / arduino-cli typically performs the 1200-bps touch
itself (boards.txt: upload.use_1200bps_touch=true), but we still pass
adafruit-nrfutil's own `-t 1200` as a safety belt so the script works whether
the host did the touch or not.

Requires Python 3.6+ and adafruit-nrfutil on PATH:
    pip3 install --user adafruit-nrfutil   # Linux / macOS

This script is intentionally small and dependency-free (only stdlib) so it runs
out of the box on a stock Python install.
"""
from __future__ import annotations
import argparse
import os
import shutil
import subprocess
import sys
import tempfile


def fail(msg: str, code: int = 2) -> "None":
    sys.stderr.write("[nius-upload] " + msg + "\n")
    sys.exit(code)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", default="dfu", choices=["dfu"], help="upload mode (only adafruit serial-DFU is supported here)")
    ap.add_argument("--hex", required=True, help="path to compiled .hex (input to adafruit-nrfutil genpkg)")
    ap.add_argument("--port", required=True, help="serial port (e.g. /dev/ttyACM0 or /dev/cu.usbmodemXXXX)")
    ap.add_argument("--vid", default="0x239A", help="bootloader USB VID (informational; not used by serial DFU)")
    ap.add_argument("--pid", default="0x00B3", help="bootloader USB PID (informational; not used by serial DFU)")
    ap.add_argument("--sd-req", default="0xFFFE", help="adafruit-nrfutil genpkg --sd-req (SoftDevice req hash; 0xFFFE = wildcard)")
    ap.add_argument("--dev-type", default="0x0052", help="adafruit-nrfutil genpkg --dev-type (0x0052 = nRF52)")
    ap.add_argument("--baud", default="115200", help="serial DFU baud (Adafruit fork uses 115200)")
    ap.add_argument("--touch", default="1200", help="open port at this baud before DFU; 0 disables")
    ap.add_argument("--nrfutil", default="adafruit-nrfutil", help="adafruit-nrfutil executable name or path")
    ap.add_argument("--verbose", action="store_true", help="pass --verbose to adafruit-nrfutil")
    args = ap.parse_args()

    if not os.path.isfile(args.hex):
        fail("input hex not found: " + args.hex)

    nrfutil = shutil.which(args.nrfutil)
    if not nrfutil:
        fail(
            "adafruit-nrfutil not found on PATH (looked for: " + args.nrfutil + ").\n"
            "Install it with:  pip3 install --user adafruit-nrfutil\n"
            "Then ensure the pip --user bin directory is on your PATH "
            "(usually ~/.local/bin on Linux, ~/Library/Python/3.x/bin on macOS).",
            code=3,
        )

    verbose_flag = ["--verbose"] if args.verbose else []

    with tempfile.TemporaryDirectory(prefix="nius_dfu_") as td:
        pkg = os.path.join(td, "app.zip")

        genpkg = [nrfutil] + verbose_flag + [
            "dfu", "genpkg",
            "--dev-type", args.dev_type,
            "--sd-req", args.sd_req,
            "--application", args.hex,
            pkg,
        ]
        if args.verbose:
            sys.stderr.write("[nius-upload] + " + " ".join(genpkg) + "\n")
        rc = subprocess.run(genpkg).returncode
        if rc != 0:
            return rc

        dfu = [nrfutil] + verbose_flag + [
            "dfu", "serial",
            "-pkg", pkg,
            "-p", args.port,
            "-b", str(args.baud),
            "--singlebank",
        ]
        try:
            if int(args.touch, 0) > 0:
                dfu += ["-t", str(args.touch)]
        except ValueError:
            pass
        if args.verbose:
            sys.stderr.write("[nius-upload] + " + " ".join(dfu) + "\n")
        rc = subprocess.run(dfu).returncode
        return rc


if __name__ == "__main__":
    sys.exit(main())
