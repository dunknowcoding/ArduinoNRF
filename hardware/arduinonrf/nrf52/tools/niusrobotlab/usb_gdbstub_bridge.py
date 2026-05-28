#!/usr/bin/env python3
"""Cross-platform USB-CDC GDB-stub bridge: TCP <-> serial proxy for cortex-debug.

Linux / macOS counterpart to usb_gdbstub_bridge.ps1. Listens on a TCP port (3335
by default), accepts one gdb / cortex-debug client, and proxies bytes to and
from the board's service CDC where the in-firmware GDB stub speaks RSP.

Cross-platform yield-on-upload IPC mirrors the PowerShell bridge: an upload
drops `<tmp>/nius_gdb_yield_<PORT>.req`; we release the COM and exit cleanly so
the upload's 1200-bps touch can reach the board.

Requires Python 3.6+ and pyserial (pip3 install --user pyserial). pyserial is
already pulled in by adafruit-nrfutil, so a host that can upload can also debug.
"""
from __future__ import annotations
import argparse
import os
import re
import socket
import sys
import tempfile
import time


def yield_request_path(port: str) -> str:
    key = re.sub(r"[^A-Za-z0-9_./-]", "_", port).upper()
    name = "nius_gdb_yield_{0}.req".format(key)
    return os.path.join(tempfile.gettempdir(), name)


def yield_requested(path: str, max_age_s: float = 45.0) -> bool:
    if not os.path.exists(path):
        return False
    try:
        age = time.time() - os.path.getmtime(path)
    except OSError:
        return False
    return age <= max_age_s


def log(msg: str) -> None:
    sys.stderr.write("[bridge] " + msg + "\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--serial-port", required=True, help="serial port (e.g. /dev/ttyACM0)")
    ap.add_argument("--tcp-port", type=int, default=3335, help="TCP listen port (default 3335)")
    ap.add_argument("--baud", type=int, default=115200, help="serial baud (default 115200)")
    ap.add_argument("--board", default="", help="board identifier (informational only)")
    args = ap.parse_args()

    try:
        import serial  # pyserial
    except ImportError:
        sys.stderr.write(
            "[bridge] pyserial is required.\n"
            "         install: pip3 install --user pyserial   (also pulled in by adafruit-nrfutil)\n"
        )
        return 3

    log("opening serial {0} @ {1} baud".format(args.serial_port, args.baud))
    try:
        ser = serial.Serial(
            port=args.serial_port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.05,
            write_timeout=3.0,
            dsrdtr=False,
            rtscts=False,
            xonxoff=False,
        )
        ser.dtr = True
        ser.rts = True
    except Exception as e:
        sys.stderr.write("[bridge] failed to open {0}: {1}\n".format(args.serial_port, e))
        return 4

    yreq = yield_request_path(args.serial_port)
    log("yield request file: " + yreq)

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        listener.bind(("127.0.0.1", args.tcp_port))
    except OSError as e:
        sys.stderr.write("[bridge] cannot bind 127.0.0.1:{0}: {1}\n".format(args.tcp_port, e))
        try:
            ser.close()
        except Exception:
            pass
        return 5
    listener.listen(1)
    listener.settimeout(0.05)

    # Banner that cortex-debug (servertype=openocd) waits for - it scans stdout
    # for /Info\s:[^\n]*Listening on port \d+ for gdb connection/i and treats the
    # server as initialized only when that exact form appears. Flush so a pipe
    # parent sees it before we block on accept().
    sys.stdout.write("Info : Listening on port {0} for gdb connections\n".format(args.tcp_port))
    sys.stdout.write(
        "USB CDC GDB stub bridge listening on tcp://127.0.0.1:{0} and forwarding to {1} @ {2} baud\n".format(
            args.tcp_port, args.serial_port, args.baud
        )
    )
    sys.stdout.write("Waiting for a GDB client connection...\n")
    sys.stdout.flush()

    client = None
    try:
        # accept-wait loop: respond to a yield request even before a client connects
        while client is None:
            if yield_requested(yreq):
                log("upload yield requested before client connect; releasing serial + exiting")
                sys.stdout.write("Info : releasing port for firmware upload\n")
                sys.stdout.flush()
                return 0
            try:
                client, _addr = listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return 6

        client.setblocking(False)
        log("gdb client connected; forwarding")
        sys.stdout.write("GDB client connected. Forwarding traffic. Press Ctrl+C to stop.\n")
        sys.stdout.flush()

        # Relay loop: poll TCP -> serial, serial -> TCP, and the yield request.
        while True:
            did_work = False

            try:
                data = client.recv(4096)
                if data == b"":
                    log("gdb client closed connection")
                    return 0
                ser.write(data)
                did_work = True
            except BlockingIOError:
                pass
            except (ConnectionResetError, OSError):
                return 0

            try:
                n = ser.in_waiting
            except Exception:
                n = 0
            if n:
                try:
                    chunk = ser.read(n)
                except Exception:
                    chunk = b""
                if chunk:
                    try:
                        client.sendall(chunk)
                        did_work = True
                    except (BrokenPipeError, ConnectionResetError, OSError):
                        return 0

            if not did_work:
                if yield_requested(yreq):
                    log("upload yield requested during session; releasing serial + exiting")
                    return 0
                time.sleep(0.005)
    except KeyboardInterrupt:
        log("interrupted")
        return 0
    finally:
        try:
            if client is not None:
                client.close()
        except Exception:
            pass
        try:
            listener.close()
        except Exception:
            pass
        try:
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
