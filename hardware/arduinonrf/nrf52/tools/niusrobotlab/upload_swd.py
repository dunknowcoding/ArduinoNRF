#!/usr/bin/env python3
"""Validated Linux/macOS OpenOCD upload for ArduinoNRF targets."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

from build_uf2 import parse_hex_segments, validate_application_layout
from upload import parse_bounded_float, run_owned_command


def fail(message: str, code: int = 2) -> "None":
    sys.stderr.write("[nius-swd] " + message + "\n")
    raise SystemExit(code)


def openocd_program_command(hex_path: Path) -> str:
    normalized = hex_path.resolve().as_posix()
    if "{" in normalized or "}" in normalized:
        raise ValueError("build path contains an unsupported Tcl brace")
    return f"telnet_port disabled; init; halt; program {{{normalized}}} verify reset; shutdown"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", required=True, type=Path)
    parser.add_argument("--script-root", required=True, type=Path)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--hex", required=True, type=Path)
    parser.add_argument("--app-start", required=True)
    parser.add_argument("--max-size", required=True)
    parser.add_argument("--ram-end", required=True)
    parser.add_argument("--timeout", default="180")
    return parser.parse_args()


def main() -> int:
    if sys.argv[1:] == ["--selftest"]:
        command = openocd_program_command(Path("firmware.hex"))
        assert "program {" in command and " verify reset; shutdown" in command
        print("upload_swd.py selftest: PASS")
        return 0

    args = parse_args()
    try:
        app_start = int(args.app_start, 0)
        max_size = int(args.max_size, 0)
        ram_end = int(args.ram_end, 0)
        timeout = parse_bounded_float(args.timeout, "OpenOCD timeout", 1.0, 600.0)
        segments = parse_hex_segments(args.hex)
        validate_application_layout(segments, app_start, max_size, ram_end)
        command = openocd_program_command(args.hex)
    except (OSError, ValueError) as error:
        fail("application image preflight failed: " + str(error))

    for label, path in (
        ("OpenOCD executable", args.tool),
        ("OpenOCD script root", args.script_root),
        ("OpenOCD target config", args.config),
    ):
        expected = path.is_dir() if label.endswith("root") else path.is_file()
        if not expected:
            fail(f"{label} not found: {path}", code=3)

    argv = [
        str(args.tool.resolve()),
        "-s",
        str(args.script_root.resolve()),
        "-f",
        str(args.config.resolve()),
        "-c",
        command,
    ]
    try:
        result = run_owned_command(argv, timeout_s=timeout)
    except subprocess.TimeoutExpired:
        fail("OpenOCD timed out; its invocation-owned process group was stopped", code=5)
    except OSError as error:
        fail("cannot launch OpenOCD: " + str(error), code=3)
    if result != 0:
        # POSIX signal exits are negative and tool-defined statuses can exceed
        # the portable shell range. Keep the diagnostic, but return a stable
        # uploader failure code to Arduino CLI.
        portable_code = result if 1 <= result <= 125 else 1
        fail(f"OpenOCD failed with exit code {result}", code=portable_code)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
