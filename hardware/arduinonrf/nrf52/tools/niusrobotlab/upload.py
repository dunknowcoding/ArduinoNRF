#!/usr/bin/env python3
"""Cross-platform serial-DFU upload for the ArduinoNRF Adafruit-fork bootloader.

Mirrors the proven Adafruit nRF52 BSP pattern (adafruit-nrfutil dfu genpkg + dfu
serial), used on Linux and macOS where the Windows-specific upload.ps1 is not
available. ArduinoNRF deliberately disables Arduino CLI's generic 1200-bps
touch and lets this identity-aware uploader perform it through
adafruit-nrfutil. That prevents a host-side port scan from selecting a peer
board during simultaneous USB re-enumeration.

Requires Python 3.7+ and adafruit-nrfutil on PATH:
    pip3 install --user adafruit-nrfutil   # Linux / macOS

This script is intentionally small and dependency-free (only stdlib) so it runs
out of the box on a stock Python install.
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

from build_uf2 import parse_hex_segments, validate_application_layout


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
        interface_number = None
        for interface in (node,) + tuple(node.parents):
            number_file = interface / "bInterfaceNumber"
            if number_file.exists():
                try:
                    interface_number = int(number_file.read_text().strip(), 16)
                except ValueError:
                    pass
                break
        return (
            int(vid_file.read_text().strip(), 16),
            int(pid_file.read_text().strip(), 16),
            serial,
            "/dev/" + tty,
            interface_number,
        )
    return None


def mac_serial_devices():
    """Return USB-backed macOS serial endpoints from the IOUSB registry tree.

    The selected /dev/cu.* endpoint must be tied to its parent USB device and
    interface number; counting VID/PID matches from system_profiler cannot tell
    two CDC interfaces on one composite device apart.
    """
    try:
        raw = subprocess.run(
            ["ioreg", "-a", "-p", "IOUSB"],
            check=True,
            capture_output=True,
            text=False,
            timeout=30,
        ).stdout
        root = plistlib.loads(raw)
    except (OSError, subprocess.SubprocessError, ValueError,
            plistlib.InvalidFileException):
        return []

    devices = []

    def first_value(value, names, default=None):
        for name in names:
            if name in value:
                return value[name]
        return default

    def as_int(value):
        if isinstance(value, int):
            return value
        text = str(value or "").strip()
        try:
            return int(text, 0)
        except ValueError:
            match = re.search(r"0x([0-9a-fA-F]+)", text)
            return int(match.group(1), 16) if match else None

    def walk(value, inherited=None):
        if not isinstance(value, dict):
            return
        current = dict(inherited or {})
        vid = as_int(first_value(value, ("idVendor", "USB Vendor ID")))
        pid = as_int(first_value(value, ("idProduct", "USB Product ID")))
        serial = first_value(
            value,
            ("USB Serial Number", "kUSBSerialNumberString", "serial_num"),
        )
        interface_number = as_int(first_value(
            value, ("bInterfaceNumber", "USB Interface Number")
        ))
        if vid is not None:
            current["vid"] = vid
        if pid is not None:
            current["pid"] = pid
        if serial is not None:
            current["serial"] = str(serial)
        if interface_number is not None:
            current["interface"] = interface_number
        port = first_value(value, ("IOCalloutDevice", "IODialinDevice"))
        if port and "vid" in current and "pid" in current:
            devices.append((
                current["vid"], current["pid"], current.get("serial", ""),
                str(port), current.get("interface"),
            ))
        for child in value.get("IORegistryEntryChildren", []):
            walk(child, current)

    for item in root if isinstance(root, list) else [root]:
        walk(item)
    return devices


def same_mac_serial_endpoint(left: str, right: str) -> bool:
    def suffix(value):
        name = os.path.basename(value)
        return re.sub(r"^(?:cu|tty)\.", "", name)
    return suffix(left) == suffix(right)


def resolve_service_serial_port(
    port: str,
    runtime_vid: int,
    runtime_pid: int,
    serial: str,
) -> str:
    """Map a selected USER CDC to interface zero on the same USB composite."""
    if sys.platform.startswith("linux"):
        selected = linux_tty_usb_identity(port)
        if not selected or (selected[0], selected[1]) != (runtime_vid, runtime_pid):
            return port
        candidates = []
        for tty_node in Path("/sys/class/tty").glob("*"):
            identity = linux_tty_usb_identity("/dev/" + tty_node.name)
            if not identity:
                continue
            if (identity[0], identity[1]) != (runtime_vid, runtime_pid):
                continue
            if serial and identity[2] != serial:
                continue
            if identity[4] == 0:
                candidates.append(identity[3])
        if len(candidates) == 1:
            return candidates[0]
        if selected[4] == 0:
            return selected[3]
        fail("cannot uniquely resolve SERVICE CDC interface zero for " + port, code=4)
    if sys.platform == "darwin":
        selected = [d for d in mac_serial_devices()
                    if same_mac_serial_endpoint(d[3], port)]
        if len(selected) != 1 or (selected[0][0], selected[0][1]) != (
                runtime_vid, runtime_pid):
            return port
        candidates = [
            d[3] for d in mac_serial_devices()
            if (d[0], d[1]) == (runtime_vid, runtime_pid)
            and (not serial or d[2] == serial) and d[4] == 0
        ]
        if len(candidates) == 1:
            return candidates[0]
        if selected[0][4] == 0:
            return selected[0][3]
        fail("cannot uniquely resolve SERVICE CDC interface zero for " + port, code=4)
    return port


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
        matches = [d for d in mac_serial_devices()
                   if same_mac_serial_endpoint(d[3], port)]
        if len(matches) != 1:
            fail(
                "cannot uniquely scope the selected macOS serial endpoint; expected "
                f"exactly one match for {port}, found {len(matches)}",
                code=4,
            )
        if (matches[0][0], matches[0][1]) not in allowed:
            fail(
                "selected serial endpoint matches neither the configured runtime nor "
                "bootloader USB identity",
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
            service = [d for d in candidates if d[4] == 0]
            if len(service) == 1 or (bool(serial) and bool(candidates)):
                return True
        elif sys.platform == "darwin":
            candidates = [
                d for d in mac_serial_devices()
                if d[0] == runtime_vid and d[1] == runtime_pid and (not serial or d[2] == serial)
            ]
            service = [d for d in candidates if d[4] == 0]
            if len(service) == 1 or (bool(serial) and bool(candidates)):
                return True
        # IORegistry enumeration is substantially more expensive than sysfs.
        # Avoid spawning it four times per second while retaining responsive
        # Linux polling.
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
    ap.add_argument("--app-start", required=True, help="required application vector address")
    ap.add_argument("--max-size", required=True, help="maximum application bytes from --app-start")
    ap.add_argument("--runtime-timeout", default="30", help="seconds to wait for identity-verified application USB")
    ap.add_argument("--sd-req", default="0xFFFE", help="adafruit-nrfutil genpkg --sd-req (SoftDevice req hash; 0xFFFE = wildcard)")
    ap.add_argument("--dev-type", default="0x0052", help="adafruit-nrfutil genpkg --dev-type (0x0052 = nRF52)")
    ap.add_argument("--baud", default="115200", help="serial DFU baud (Adafruit fork uses 115200)")
    ap.add_argument("--touch", default="1200", help="open port at this baud before DFU; 0 disables")
    ap.add_argument("--nrfutil", default="adafruit-nrfutil", help="adafruit-nrfutil executable name or path")
    ap.add_argument("--verbose", action="store_true", help="pass --verbose to adafruit-nrfutil")
    args = ap.parse_args()

    if args.vid.strip().lower() == "auto" or args.pid.strip().lower() == "auto":
        fail(
            "this board has no canonical USB bootloader identity; select an "
            "explicit Bootloader / DFU entry before uploading on Linux or macOS",
            code=2,
        )

    try:
        boot_vid = parse_usb_id(args.vid)
        boot_pid = parse_usb_id(args.pid)
        runtime_vid = parse_usb_id(args.runtime_vid)
        runtime_pid = parse_usb_id(args.runtime_pid)
        app_start = int(args.app_start, 0)
        max_size = int(args.max_size, 0)
        runtime_timeout = float(args.runtime_timeout)
    except ValueError:
        fail("invalid USB identity, application range, or runtime timeout", code=2)

    if not os.path.isfile(args.hex):
        fail("input hex not found: " + args.hex)
    try:
        validate_application_layout(
            parse_hex_segments(Path(args.hex)), app_start, max_size
        )
    except (OSError, ValueError) as error:
        fail("application image preflight failed: " + str(error), code=2)

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
    control_port = resolve_service_serial_port(
        args.port, runtime_vid, runtime_pid, target_serial
    )
    if control_port != args.port:
        sys.stderr.write(
            f"[nius-upload] selected {args.port}; using same-device SERVICE CDC "
            f"{control_port} for bootloader touch and DFU\n"
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
            "-p", control_port,
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
