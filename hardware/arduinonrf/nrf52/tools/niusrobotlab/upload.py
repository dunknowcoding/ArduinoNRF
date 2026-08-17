#!/usr/bin/env python3
"""Cross-platform serial-DFU upload for the ArduinoNRF Adafruit-fork bootloader.

Mirrors the proven Adafruit nRF52 BSP pattern (adafruit-nrfutil dfu genpkg + dfu
serial), used on Linux and macOS where the Windows-specific upload.ps1 is not
available. ArduinoNRF deliberately disables Arduino CLI's generic 1200-bps
touch and lets this identity-aware uploader perform it through
adafruit-nrfutil. That prevents a host-side port scan from selecting a peer
board during simultaneous USB re-enumeration.

Requires Python 3.6+ and adafruit-nrfutil on PATH:
    pip3 install --user adafruit-nrfutil   # Linux / macOS

This script is intentionally small and dependency-free (only stdlib) so it runs
out of the box on a stock Python install.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time


def fail(msg: str, code: int = 2) -> "None":
    sys.stderr.write("[nius-upload] " + msg + "\n")
    sys.exit(code)


def parse_usb_id(value: str) -> int:
    return int(value, 0)


def linux_tty_usb_identity(port: str):
    tty = os.path.basename(os.path.realpath(port))
    node = Path("/sys/class/tty") / tty / "device"
    if not node.exists():
        return None
    node = node.resolve()
    for parent in (node,) + tuple(node.parents):
        vid_file = parent / "idVendor"
        pid_file = parent / "idProduct"
        if not vid_file.exists() or not pid_file.exists():
            continue
        serial_file = parent / "serial"
        serial = serial_file.read_text(errors="ignore").strip() if serial_file.exists() else ""
        return (
            int(vid_file.read_text().strip(), 16),
            int(pid_file.read_text().strip(), 16),
            serial,
            "/dev/" + tty,
        )
    return None


def mac_usb_devices():
    try:
        raw = subprocess.run(
            ["system_profiler", "SPUSBDataType", "-json"],
            check=True,
            capture_output=True,
            text=True,
            timeout=30,
        ).stdout
        root = json.loads(raw)
    except (OSError, subprocess.SubprocessError, ValueError):
        return []

    devices = []

    def walk(value):
        if isinstance(value, dict):
            vid_text = str(value.get("vendor_id", ""))
            pid_text = str(value.get("product_id", ""))
            vid_match = re.search(r"0x([0-9a-fA-F]{4})", vid_text)
            pid_match = re.search(r"0x([0-9a-fA-F]{4})", pid_text)
            if vid_match and pid_match:
                devices.append(
                    (
                        int(vid_match.group(1), 16),
                        int(pid_match.group(1), 16),
                        str(value.get("serial_num", "")),
                        str(value.get("_name", "USB device")),
                    )
                )
            for child in value.values():
                walk(child)
        elif isinstance(value, list):
            for child in value:
                walk(child)

    walk(root)
    return devices


def capture_target_serial(
    port: str,
    boot_vid: int,
    boot_pid: int,
    runtime_vid: int,
    runtime_pid: int,
) -> str:
    allowed = {(boot_vid, boot_pid), (runtime_vid, runtime_pid)}
    if sys.platform.startswith("linux"):
        identity = linux_tty_usb_identity(port)
        if not identity:
            fail("selected serial port has no USB identity: " + port, code=4)
        if (identity[0], identity[1]) not in allowed:
            fail(
                "selected serial port matches neither the configured runtime nor "
                "bootloader identity: "
                f"got {identity[0]:04X}:{identity[1]:04X}; expected "
                f"{runtime_vid:04X}:{runtime_pid:04X} or {boot_vid:04X}:{boot_pid:04X}",
                code=4,
            )
        return identity[2]
    if sys.platform == "darwin":
        matches = [d for d in mac_usb_devices() if (d[0], d[1]) in allowed]
        if len(matches) != 1:
            fail(
                "cannot uniquely scope the selected macOS target; expected exactly one "
                f"runtime/bootloader match, found {len(matches)}",
                code=4,
            )
        return matches[0][2]
    fail("unsupported host for identity-verified serial DFU: " + sys.platform, code=4)


def wait_for_runtime(runtime_vid: int, runtime_pid: int, serial: str, timeout_s: float) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if sys.platform.startswith("linux"):
            candidates = []
            for tty_node in Path("/sys/class/tty").glob("*"):
                identity = linux_tty_usb_identity("/dev/" + tty_node.name)
                if identity and identity[0] == runtime_vid and identity[1] == runtime_pid:
                    if not serial or identity[2] == serial:
                        candidates.append(identity)
            if len(candidates) == 1:
                return True
        elif sys.platform == "darwin":
            candidates = [
                d for d in mac_usb_devices()
                if d[0] == runtime_vid and d[1] == runtime_pid and (not serial or d[2] == serial)
            ]
            if len(candidates) == 1:
                return True
        # system_profiler is substantially more expensive than sysfs. Avoid
        # spawning it four times per second while retaining responsive Linux
        # polling.
        time.sleep(1.0 if sys.platform == "darwin" else 0.25)
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", default="dfu", choices=["dfu"], help="upload mode (only adafruit serial-DFU is supported here)")
    ap.add_argument("--hex", required=True, help="path to compiled .hex (input to adafruit-nrfutil genpkg)")
    ap.add_argument("--port", required=True, help="serial port (e.g. /dev/ttyACM0 or /dev/cu.usbmodemXXXX)")
    ap.add_argument("--vid", default="0x239A", help="expected bootloader USB VID")
    ap.add_argument("--pid", default="0x00B3", help="expected bootloader USB PID")
    ap.add_argument("--runtime-vid", required=True, help="expected application USB VID")
    ap.add_argument("--runtime-pid", required=True, help="expected application USB PID")
    ap.add_argument("--runtime-timeout", default="30", help="seconds to wait for identity-verified application USB")
    ap.add_argument("--sd-req", default="0xFFFE", help="adafruit-nrfutil genpkg --sd-req (SoftDevice req hash; 0xFFFE = wildcard)")
    ap.add_argument("--dev-type", default="0x0052", help="adafruit-nrfutil genpkg --dev-type (0x0052 = nRF52)")
    ap.add_argument("--baud", default="115200", help="serial DFU baud (Adafruit fork uses 115200)")
    ap.add_argument("--touch", default="1200", help="open port at this baud before DFU; 0 disables")
    ap.add_argument("--nrfutil", default="adafruit-nrfutil", help="adafruit-nrfutil executable name or path")
    ap.add_argument("--verbose", action="store_true", help="pass --verbose to adafruit-nrfutil")
    args = ap.parse_args()

    try:
        boot_vid = parse_usb_id(args.vid)
        boot_pid = parse_usb_id(args.pid)
        runtime_vid = parse_usb_id(args.runtime_vid)
        runtime_pid = parse_usb_id(args.runtime_pid)
        runtime_timeout = float(args.runtime_timeout)
    except ValueError:
        fail("invalid USB identity or runtime timeout", code=2)

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

    target_serial = capture_target_serial(
        args.port, boot_vid, boot_pid, runtime_vid, runtime_pid
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
        try:
            rc = subprocess.run(genpkg, timeout=120).returncode
        except subprocess.TimeoutExpired:
            fail("adafruit-nrfutil genpkg timed out", code=5)
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
        try:
            rc = subprocess.run(dfu, timeout=240).returncode
        except subprocess.TimeoutExpired:
            fail("adafruit serial DFU timed out", code=5)
        if rc != 0:
            return rc
        if not wait_for_runtime(runtime_vid, runtime_pid, target_serial, runtime_timeout):
            fail(
                "transfer completed, but the selected board did not enumerate as runtime "
                f"{runtime_vid:04X}:{runtime_pid:04X}",
                code=6,
            )
        return 0


if __name__ == "__main__":
    sys.exit(main())
