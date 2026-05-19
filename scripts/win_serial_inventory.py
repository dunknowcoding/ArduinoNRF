#!/usr/bin/env python3
"""
Windows serial / Ports-class inventory for CDC troubleshooting.

Uses (in order):
  1) pyserial's list_ports if package is installed (optional: pip install pyserial).
  2) Otherwise Win32_SerialPort via PowerShell (COM + PNPDeviceID), no extra deps.

Recommended invocation (your conda env):
  conda run -n IronEngineWorld python scripts/win_serial_inventory.py
  conda run -n IronEngineWorld python scripts/win_serial_inventory.py --json
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys


def _run_ps(command: str) -> str:
    r = subprocess.run(
        [
            "powershell",
            "-NoProfile",
            "-NoLogo",
            "-Command",
            command,
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if r.returncode != 0:
        raise RuntimeError(r.stderr or r.stdout or f"exit {r.returncode}")
    return r.stdout.strip()


def inventory_via_pyserial() -> list[dict]:
    from serial.tools import list_ports

    rows: list[dict] = []
    for p in list_ports.comports():
        rows.append(
            {
                "device": p.device,
                "description": p.description or "",
                "hwid": p.hwid or "",
                "vid": p.vid,
                "pid": p.pid,
                "serial_number": p.serial_number or "",
                "manufacturer": p.manufacturer or "",
                "source": "pyserial",
            }
        )
    return rows


def inventory_via_cim() -> list[dict]:
    ps = r"""
$ports = @(Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue)
$output = foreach ($p in $ports) {
    [pscustomobject]@{
        device      = [string]$p.DeviceID
        description = [string]$p.Description
        pnp_id      = [string]$p.PNPDeviceID
        source      = 'Win32_SerialPort'
    }
}
$output | ConvertTo-Json -Compress -Depth 4
"""
    raw = _run_ps(ps)
    if not raw:
        return []
    data = json.loads(raw)
    if isinstance(data, dict):
        return [data]
    return list(data)


def inventory_via_pnp_ports() -> list[dict]:
    """Friendly names often include (COMx); useful when Win32_SerialPort is sparse."""
    ps = r"""
$dev = @(Get-PnpDevice -PresentOnly -Class 'Ports' -ErrorAction SilentlyContinue | ForEach-Object {
    [pscustomobject]@{
        friendly_name = [string]$_.FriendlyName
        instance_id   = [string]$_.InstanceId
        status        = [string]$_.Status
        source        = 'Get-PnpDevice-Ports'
    }
})
$dev | ConvertTo-Json -Compress -Depth 4
"""
    raw = _run_ps(ps)
    if not raw:
        return []
    data = json.loads(raw)
    if isinstance(data, dict):
        return [data]
    return list(data)


def main() -> int:
    parser = argparse.ArgumentParser(description="Windows COM / USB-serial inventory")
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit single JSON object with keys pyserial, win32_serial, pnp_ports",
    )
    parser.add_argument(
        "--cim-only",
        action="store_true",
        help="Skip pyserial; only Win32_SerialPort + PnpDevice Ports",
    )
    args = parser.parse_args()

    pyserial_rows: list[dict] = []
    if not args.cim_only:
        try:
            pyserial_rows = inventory_via_pyserial()
        except ImportError:
            pyserial_rows = []

    cim_rows = inventory_via_cim()
    pnp_rows = inventory_via_pnp_ports()

    if args.json:
        print(
            json.dumps(
                {"pyserial": pyserial_rows, "win32_serial": cim_rows, "pnp_ports": pnp_rows},
                indent=2,
                ensure_ascii=False,
            )
        )
        return 0

    if pyserial_rows:
        print("[pyserial]")
        for row in pyserial_rows:
            vid = row.get("vid")
            pid = row.get("pid")
            print(
                f"  {row['device']}\tdesc={row.get('description','')}\tvid={vid}\tpid={pid}\n"
                f"    hwid={row.get('hwid','')}"
            )
        print()

    print("[Win32_SerialPort]")
    if not cim_rows:
        print("  (none)")
    else:
        for row in cim_rows:
            print(f"  {row.get('device','')}\t{row.get('description','')}")
            print(f"    PNP={row.get('pnp_id','')}")
    print()

    print("[Get-PnpDevice Class=Ports]")
    if not pnp_rows:
        print("  (none)")
    else:
        for row in pnp_rows:
            print(f"  {row.get('friendly_name','')}")
            print(f"    InstanceId={row.get('instance_id','')}\tStatus={row.get('status','')}")

    if not args.cim_only:
        try:
            import serial  # noqa: F401
        except ImportError:
            print(
                "\nTip: install pyserial in IronEngineWorld for richer VID/PID parsing:\n"
                "  conda run -n IronEngineWorld pip install pyserial",
                file=sys.stderr,
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
