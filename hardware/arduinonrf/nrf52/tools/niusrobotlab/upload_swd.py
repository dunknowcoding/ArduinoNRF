#!/usr/bin/env python3
"""Validated Linux/macOS OpenOCD upload for ArduinoNRF targets."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

from build_uf2 import (
    parse_hex_segments,
    validate_application_layout,
    validate_bootloader_layout,
)
from upload import parse_bounded_float, run_owned_command


def fail(message: str, code: int = 2) -> "None":
    sys.stderr.write("[nius-swd] " + message + "\n")
    raise SystemExit(code)


def openocd_tcl_word(value: str) -> str:
    if any(ord(character) < 0x20 or ord(character) == 0x7F for character in value):
        raise ValueError("OpenOCD command value contains a control character")
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("$", "\\$")
        .replace("[", "\\[")
        .replace("]", "\\]")
    )
    return f'"{escaped}"'


def openocd_program_command(hex_path: Path, recover: bool = False) -> str:
    firmware = openocd_tcl_word(hex_path.resolve().as_posix())
    prefix = "telnet_port disabled; init; halt; "
    if recover:
        prefix += "nrf52_recover; reset halt; "
    return f"{prefix}program {firmware} verify reset; shutdown"


def optional_probe_identity() -> str:
    identity = os.environ.get("NIUS_CMSIS_DAP_SERIAL", "").strip()
    if not identity:
        return ""
    if len(identity) > 128 or any(
        ord(character) < 0x20 or ord(character) == 0x7F for character in identity
    ):
        raise ValueError(
            "NIUS_CMSIS_DAP_SERIAL must be at most 128 characters without control characters"
        )
    return identity


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--mode", choices=("application", "bootloader"), default="application"
    )
    parser.add_argument("--tool", required=True, type=Path)
    parser.add_argument("--script-root", required=True, type=Path)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--hex", required=True, type=Path)
    parser.add_argument("--app-start")
    parser.add_argument("--max-size")
    parser.add_argument("--ram-end", required=True)
    parser.add_argument("--flash-end")
    parser.add_argument("--timeout", default="180")
    return parser.parse_args()


def main() -> int:
    if sys.argv[1:] == ["--selftest"]:
        command = openocd_program_command(Path("firm${ware}.hex"))
        assert "program \"" in command and "\\$" in command
        assert "nrf52_recover" in openocd_program_command(
            Path("bootloader.hex"), recover=True
        )
        print("upload_swd.py selftest: PASS")
        return 0

    args = parse_args()
    try:
        ram_end = int(args.ram_end, 0)
        timeout = parse_bounded_float(args.timeout, "OpenOCD timeout", 1.0, 600.0)
        segments = parse_hex_segments(args.hex)
        if args.mode == "bootloader":
            if args.flash_end is None:
                raise ValueError("--flash-end is required in bootloader mode")
            if args.app_start is not None or args.max_size is not None:
                raise ValueError(
                    "application layout arguments cannot accompany bootloader mode"
                )
            validate_bootloader_layout(segments, int(args.flash_end, 0), ram_end)
            command = openocd_program_command(args.hex, recover=True)
        else:
            if args.app_start is None or args.max_size is None:
                raise ValueError(
                    "--app-start and --max-size are required in application mode"
                )
            validate_application_layout(
                segments, int(args.app_start, 0), int(args.max_size, 0), ram_end
            )
            command = openocd_program_command(args.hex)
        probe_identity = optional_probe_identity()
    except (OSError, ValueError) as error:
        fail(f"{args.mode} image preflight failed: {error}")

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
    ]
    if probe_identity:
        argv.extend(
            ["-c", f"adapter serial {openocd_tcl_word(probe_identity)}"]
        )
    argv.extend(["-c", command])
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
