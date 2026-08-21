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
from contextlib import contextmanager
import hashlib
import math
import os
from pathlib import Path
import plistlib
import re
import signal
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
    parsed = int(value, 0)
    if not 0 <= parsed <= 0xFFFF:
        raise ValueError("USB VID/PID must fit 16 bits")
    return parsed


def parse_bounded_int(value: str, name: str, minimum: int, maximum: int) -> int:
    parsed = int(value, 0)
    if not minimum <= parsed <= maximum:
        raise ValueError(f"{name} must be in [{minimum}, {maximum}]")
    return parsed


def parse_bounded_float(value: str, name: str, minimum: float, maximum: float) -> float:
    parsed = float(value)
    if not math.isfinite(parsed) or not minimum <= parsed <= maximum:
        raise ValueError(f"{name} must be finite and in [{minimum}, {maximum}]")
    return parsed


def run_owned_command(argv, timeout_s: float) -> int:
    """Run one uploader child and contain its complete process group on timeout.

    The Linux/macOS uploader must never leave a converter or transport helper
    holding the selected tty after its bounded parent command has failed.  A new
    session gives this invocation an exact ownership boundary; timeout cleanup
    signals only that group and preserves the original TimeoutExpired failure.
    """
    if os.name != "posix":
        # This file is not the Windows upload route.  Keeping the direct fallback
        # makes its pure self-test importable there without pretending to provide
        # Windows process-tree ownership.
        return subprocess.run(argv, timeout=timeout_s).returncode

    process = subprocess.Popen(argv, start_new_session=True, close_fds=True)
    try:
        return process.wait(timeout=timeout_s)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            process.wait(timeout=1.0)
        except subprocess.TimeoutExpired:
            pass
        # The direct child may exit on SIGTERM while a grandchild that inherited
        # the tty ignores it.  Kill the still-owned group unconditionally; ESRCH
        # means the whole group already ended.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait()
        raise


class UploadBusyError(RuntimeError):
    pass


@contextmanager
def exclusive_target_lock(identity: str):
    """Hold a host-local advisory lock for one physical target."""
    import fcntl

    token = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:24]
    path = Path(tempfile.gettempdir()) / f"arduinonrf-upload-{token}.lock"
    flags = os.O_CREAT | os.O_RDWR
    if hasattr(os, "O_CLOEXEC"):
        flags |= os.O_CLOEXEC
    descriptor = os.open(path, flags, 0o600)
    handle = os.fdopen(descriptor, "a+")
    try:
        try:
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise UploadBusyError(
                "another upload owns the selected physical target"
            ) from error
        yield
    finally:
        try:
            fcntl.flock(handle.fileno(), fcntl.LOCK_UN)
        finally:
            handle.close()


@contextmanager
def upload_target_lock(identity: str):
    """Convert lock contention into a stable uploader diagnostic."""
    try:
        with exclusive_target_lock(identity):
            yield
    except UploadBusyError as error:
        fail(str(error), code=7)


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
            str(parent),
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
        location_id = as_int(first_value(
            value, ("locationID", "locationId", "USB Location ID")
        ))
        if vid is not None:
            current["vid"] = vid
        if pid is not None:
            current["pid"] = pid
        if serial is not None:
            current["serial"] = str(serial)
        if interface_number is not None:
            current["interface"] = interface_number
        if location_id is not None:
            current["stable_id"] = f"location:{location_id:08x}"
        port = first_value(value, ("IOCalloutDevice", "IODialinDevice"))
        if port and "vid" in current and "pid" in current:
            devices.append((
                current["vid"], current["pid"], current.get("serial", ""),
                str(port), current.get("interface"), current.get("stable_id", ""),
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


def matches_target_scope(identity, serial: str, stable_id: str) -> bool:
    """Prefer physical USB topology; use serial only when topology is unavailable."""
    if stable_id:
        return len(identity) > 5 and identity[5] == stable_id
    if serial:
        return identity[2] == serial
    return False


def same_captured_target(
    expected_serial: str,
    expected_stable_id: str,
    current_serial: str,
    current_stable_id: str,
) -> bool:
    """Compare a target after lock acquisition using the strongest captured key."""
    if expected_stable_id:
        return current_stable_id == expected_stable_id
    return bool(expected_serial) and current_serial == expected_serial


def advance_runtime_stability(ready: bool, now: float, ready_since, stable_s: float):
    if not ready:
        return None, False
    started = now if ready_since is None else ready_since
    return started, now - started >= stable_s


def runtime_endpoint_ready(candidates) -> bool:
    """Require the maintenance CDC when interface metadata is available.

    Some hosts omit bInterfaceNumber from their registry view; in that case an
    identity-scoped runtime tty is the strongest available evidence. If the host
    does expose interface numbers, however, a user CDC alone must not be mistaken
    for a complete upload/maintenance path.
    """
    if not candidates:
        return False
    known_interfaces = [identity[4] for identity in candidates if identity[4] is not None]
    if not known_interfaces:
        return True
    return known_interfaces.count(0) == 1


def resolve_service_serial_port(
    port: str,
    runtime_vid: int,
    runtime_pid: int,
    serial: str,
    stable_id: str,
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
            if not matches_target_scope(identity, serial, stable_id):
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
            and matches_target_scope(d, serial, stable_id) and d[4] == 0
        ]
        if len(candidates) == 1:
            return candidates[0]
        if selected[0][4] == 0:
            return selected[0][3]
        fail("cannot uniquely resolve SERVICE CDC interface zero for " + port, code=4)
    return port


def capture_target_identity(
    port: str,
    boot_vid: int,
    boot_pid: int,
    runtime_vid: int,
    runtime_pid: int,
) -> tuple[str, str]:
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
        if not identity[2] and not identity[5]:
            fail("selected serial port has no stable USB topology or serial identity", code=4)
        return identity[2], identity[5]
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
        if not matches[0][2] and not matches[0][5]:
            fail("selected serial endpoint has no stable USB topology or serial identity", code=4)
        return matches[0][2], matches[0][5]
    fail("unsupported host for identity-verified serial DFU: " + sys.platform, code=4)


def wait_for_runtime(
    runtime_vid: int,
    runtime_pid: int,
    serial: str,
    stable_id: str,
    timeout_s: float,
    stable_s: float,
) -> bool:
    deadline = time.monotonic() + timeout_s
    ready_since = None
    while time.monotonic() < deadline:
        ready = False
        if sys.platform.startswith("linux"):
            candidates = []
            for tty_node in Path("/sys/class/tty").glob("*"):
                identity = linux_tty_usb_identity("/dev/" + tty_node.name)
                if identity and identity[0] == runtime_vid and identity[1] == runtime_pid:
                    if matches_target_scope(identity, serial, stable_id):
                        candidates.append(identity)
            ready = runtime_endpoint_ready(candidates)
        elif sys.platform == "darwin":
            candidates = [
                d for d in mac_serial_devices()
                if d[0] == runtime_vid and d[1] == runtime_pid
                and matches_target_scope(d, serial, stable_id)
            ]
            ready = runtime_endpoint_ready(candidates)
        ready_since, stable = advance_runtime_stability(
            ready, time.monotonic(), ready_since, stable_s
        )
        if stable:
            return True
        # IORegistry enumeration is substantially more expensive than sysfs.
        # Avoid spawning it four times per second while retaining responsive
        # Linux polling.
        time.sleep(1.0 if sys.platform == "darwin" else 0.25)
    return False


def main() -> int:
    if sys.argv[1:] == ["--selftest"]:
        serialless = (0x239A, 0x00B3, "", "/dev/ttyACM0", 0, "/sys/devices/usb1/1-2")
        changed_serial = (0x239A, 0x0001, "runtime", "/dev/ttyACM1", 0, "/sys/devices/usb1/1-2")
        peer = (0x239A, 0x0001, "runtime", "/dev/ttyACM2", 0, "/sys/devices/usb1/1-3")
        assert matches_target_scope(serialless, "", "/sys/devices/usb1/1-2")
        assert matches_target_scope(changed_serial, "bootloader", "/sys/devices/usb1/1-2")
        assert not matches_target_scope(peer, "bootloader", "/sys/devices/usb1/1-2")
        assert matches_target_scope(changed_serial, "runtime", "")
        assert not matches_target_scope(changed_serial, "peer", "")
        assert same_captured_target(
            "bootloader", "/sys/devices/usb1/1-2",
            "runtime", "/sys/devices/usb1/1-2",
        )
        assert not same_captured_target(
            "bootloader", "/sys/devices/usb1/1-2",
            "bootloader", "/sys/devices/usb1/1-3",
        )
        assert same_captured_target("runtime", "", "runtime", "")
        assert not same_captured_target("runtime", "", "peer", "")
        assert runtime_endpoint_ready([changed_serial])
        assert runtime_endpoint_ready([
            (0x239A, 0x0001, "runtime", "/dev/ttyACM1", None, "scope")
        ])
        assert not runtime_endpoint_ready([
            (0x239A, 0x0001, "runtime", "/dev/ttyACM2", 2, "scope")
        ])
        assert not runtime_endpoint_ready([
            (0x239A, 0x0001, "runtime", "/dev/ttyACM1", 0, "scope"),
            (0x239A, 0x0001, "runtime", "/dev/ttyACM2", 0, "scope"),
        ])
        since, stable = advance_runtime_stability(True, 1.0, None, 0.3)
        assert since == 1.0 and not stable
        since, stable = advance_runtime_stability(False, 1.2, since, 0.3)
        assert since is None and not stable
        since, stable = advance_runtime_stability(True, 2.0, since, 0.3)
        since, stable = advance_runtime_stability(True, 2.31, since, 0.3)
        assert since == 2.0 and stable
        assert parse_usb_id("0xffff") == 0xFFFF
        assert parse_bounded_int("1200", "touch baud", 0, 4_000_000) == 1200
        assert parse_bounded_float("0.3", "stable duration", 0.0, 5.0) == 0.3
        for rejected in ("nan", "inf", "-1"):
            try:
                parse_bounded_float(rejected, "timeout", 0.1, 600.0)
            except ValueError:
                pass
            else:
                raise AssertionError("invalid timeout accepted: " + rejected)
        try:
            parse_usb_id("0x10000")
        except ValueError:
            pass
        else:
            raise AssertionError("oversized USB identity accepted")
        if os.name == "posix":
            lock_id = "upload-selftest-" + str(os.getpid())
            with exclusive_target_lock(lock_id):
                try:
                    with exclusive_target_lock(lock_id):
                        raise AssertionError("duplicate target lock acquired")
                except UploadBusyError:
                    pass
            try:
                run_owned_command(
                    [sys.executable, "-c", "import time; time.sleep(10)"], 0.05
                )
            except subprocess.TimeoutExpired:
                pass
            else:
                raise AssertionError("owned command timeout was not enforced")
        print("upload.py selftest: PASS")
        return 0

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", default="dfu", choices=["dfu"], help="upload mode (only adafruit serial-DFU is supported here)")
    ap.add_argument("--hex", required=True, help="path to compiled .hex (input to adafruit-nrfutil genpkg)")
    ap.add_argument("--port", required=True, help="serial port (e.g. /dev/ttyACM0 or /dev/cu.usbmodemXXXX)")
    ap.add_argument("--vid", default="0x239A", help="expected bootloader USB VID")
    ap.add_argument("--pid", default="0x00B3", help="expected bootloader USB PID")
    ap.add_argument("--runtime-vid", required=True, help="expected application USB VID")
    ap.add_argument("--runtime-pid", required=True, help="expected application USB PID")
    ap.add_argument(
        "--bootloader-mode",
        default="adafruit-dfu",
        choices=["adafruit-dfu", "uf2", "nordic-dfu", "auto"],
        help="selected board bootloader transport contract",
    )
    ap.add_argument("--app-start", required=True, help="required application vector address")
    ap.add_argument("--max-size", required=True, help="maximum application bytes from --app-start")
    ap.add_argument("--runtime-timeout", default="30", help="seconds to wait for identity-verified application USB")
    ap.add_argument("--runtime-stable-ms", default="300", help="continuous milliseconds the same application USB identity must remain present")
    ap.add_argument("--sd-req", default="0xFFFE", help="adafruit-nrfutil genpkg --sd-req (SoftDevice req hash; 0xFFFE = wildcard)")
    ap.add_argument("--dev-type", default="0x0052", help="adafruit-nrfutil genpkg --dev-type (0x0052 = nRF52)")
    ap.add_argument("--baud", default="115200", help="serial DFU baud (Adafruit fork uses 115200)")
    ap.add_argument("--touch", default="1200", help="open port at this baud before DFU; 0 disables")
    ap.add_argument("--nrfutil", default="adafruit-nrfutil", help="adafruit-nrfutil executable name or path")
    ap.add_argument("--verbose", action="store_true", help="pass --verbose to adafruit-nrfutil")
    args = ap.parse_args()

    # This wrapper implements the Adafruit serial protocol. Adafruit-compatible
    # UF2 bootloaders expose the same serial DFU service and are safe here, but
    # Nordic USB DFU is a different protocol and auto-detection would require
    # mutating discovery after the runtime endpoint disappears. Reject both
    # before image or USB access instead of silently invoking the wrong tool.
    if args.bootloader_mode == "auto":
        fail(
            "select an explicit Bootloader / DFU layout on Linux or macOS; "
            "transport auto-detection is intentionally disabled",
            code=2,
        )
    if args.bootloader_mode == "nordic-dfu":
        fail(
            "Nordic USB DFU is not implemented by the Adafruit serial uploader; "
            "select a verified UF2/Adafruit-DFU layout or an SWD upload method",
            code=2,
        )

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
        if not 0 <= app_start <= 0xFFFF_FFFF or not 1 <= max_size <= 0x1_0000_0000:
            raise ValueError("invalid application range")
        if app_start + max_size > 0x1_0000_0000:
            raise ValueError("application range exceeds the 32-bit address space")
        runtime_timeout = parse_bounded_float(
            args.runtime_timeout, "runtime timeout", 0.1, 600.0
        )
        runtime_stable_s = parse_bounded_float(
            args.runtime_stable_ms, "runtime stable duration", 0.0, 5_000.0
        ) / 1000.0
        if runtime_stable_s >= runtime_timeout:
            raise ValueError("runtime stable duration must be shorter than runtime timeout")
        baud = parse_bounded_int(args.baud, "DFU baud", 1, 4_000_000)
        touch = parse_bounded_int(args.touch, "touch baud", 0, 4_000_000)
        dev_type = parse_bounded_int(args.dev_type, "device type", 0, 0xFFFF)
        sd_req = parse_bounded_int(args.sd_req, "SoftDevice requirement", 0, 0xFFFF)
    except ValueError as error:
        fail("invalid upload contract: " + str(error), code=2)

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

    target_serial, target_stable_id = capture_target_identity(
        args.port, boot_vid, boot_pid, runtime_vid, runtime_pid
    )
    control_port = resolve_service_serial_port(
        args.port, runtime_vid, runtime_pid, target_serial, target_stable_id
    )
    if control_port != args.port:
        sys.stderr.write(
            f"[nius-upload] selected {args.port}; using same-device SERVICE CDC "
            f"{control_port} for bootloader touch and DFU\n"
        )

    verbose_flag = ["--verbose"] if args.verbose else []
    lock_identity = target_stable_id or ("serial:" + target_serial)

    with upload_target_lock(lock_identity), tempfile.TemporaryDirectory(prefix="nius_dfu_") as td:
        # Identity discovery happens before the lock only to derive its stable key.
        # Re-prove the selected endpoint after acquiring ownership so a detach,
        # renumber, or peer replacement in that window cannot redirect the transfer.
        current_serial, current_stable_id = capture_target_identity(
            control_port, boot_vid, boot_pid, runtime_vid, runtime_pid
        )
        if not same_captured_target(
            target_serial, target_stable_id, current_serial, current_stable_id
        ):
            fail(
                "selected physical target changed before the upload lock was acquired; "
                "no touch or transfer was attempted",
                code=4,
            )
        locked_control_port = resolve_service_serial_port(
            control_port, runtime_vid, runtime_pid, target_serial, target_stable_id
        )
        if locked_control_port != control_port:
            sys.stderr.write(
                f"[nius-upload] maintenance endpoint changed before transfer; "
                f"using same-device SERVICE CDC {locked_control_port}\n"
            )
            control_port = locked_control_port

        pkg = os.path.join(td, "app.zip")

        genpkg = [nrfutil] + verbose_flag + [
            "dfu", "genpkg",
            "--dev-type", f"0x{dev_type:04X}",
            "--sd-req", f"0x{sd_req:04X}",
            "--application", args.hex,
            pkg,
        ]
        if args.verbose:
            sys.stderr.write("[nius-upload] + " + " ".join(genpkg) + "\n")
        try:
            rc = run_owned_command(genpkg, timeout_s=120)
        except subprocess.TimeoutExpired:
            fail("adafruit-nrfutil genpkg timed out", code=5)
        except OSError as error:
            fail("cannot run adafruit-nrfutil genpkg: " + str(error), code=3)
        if rc != 0:
            return rc

        dfu = [nrfutil] + verbose_flag + [
            "dfu", "serial",
            "-pkg", pkg,
            "-p", control_port,
            "-b", str(baud),
            "--singlebank",
        ]
        if touch > 0:
            dfu += ["-t", str(touch)]
        if args.verbose:
            sys.stderr.write("[nius-upload] + " + " ".join(dfu) + "\n")
        try:
            rc = run_owned_command(dfu, timeout_s=240)
        except subprocess.TimeoutExpired:
            fail("adafruit serial DFU timed out", code=5)
        except OSError as error:
            fail("cannot run adafruit serial DFU: " + str(error), code=3)
        if rc != 0:
            return rc
        if not wait_for_runtime(
            runtime_vid, runtime_pid, target_serial, target_stable_id,
            runtime_timeout, runtime_stable_s
        ):
            fail(
                "transfer completed, but the selected board did not remain enumerated as runtime "
                f"{runtime_vid:04X}:{runtime_pid:04X}",
                code=6,
            )
        return 0


if __name__ == "__main__":
    sys.exit(main())
