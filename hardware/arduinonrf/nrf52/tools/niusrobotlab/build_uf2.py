from __future__ import annotations

import argparse
import struct
from pathlib import Path


UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
UF2_PAYLOAD_SIZE = 256
UF2_BLOCK_SIZE = 512


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert an Intel HEX file into a UF2 image.")
    parser.add_argument("--input-hex", required=True, type=Path)
    parser.add_argument("--output-uf2", required=True, type=Path)
    parser.add_argument("--family-id", required=True)
    return parser.parse_args()


def parse_hex_segments(path: Path) -> list[tuple[int, bytes]]:
    upper = 0
    segments: list[tuple[int, bytes]] = []

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise ValueError(f"Invalid HEX record: {line}")

        record = bytes.fromhex(line[1:])
        length = record[0]
        address = (record[1] << 8) | record[2]
        record_type = record[3]
        data = record[4 : 4 + length]

        if record_type == 0x00:
            absolute = upper + address
            segments.append((absolute, data))
        elif record_type == 0x01:
            break
        elif record_type == 0x02:
            # Extended Segment Address: base = (16-bit segment) << 4. Used by
            # arm-none-eabi-objcopy when the load address fits in 20 bits, e.g.
            # 0x26000 on the ProMicro. Without this the absolute address is
            # truncated and the firmware is written to the wrong flash location.
            upper = int.from_bytes(data, "big") << 4
        elif record_type == 0x04:
            upper = int.from_bytes(data, "big") << 16

    if not segments:
        raise ValueError(f"No data records found in {path}")

    segments.sort(key=lambda item: item[0])
    merged: list[tuple[int, bytearray]] = []
    for address, chunk in segments:
        if merged and merged[-1][0] + len(merged[-1][1]) == address:
            merged[-1][1].extend(chunk)
        else:
            merged.append((address, bytearray(chunk)))

    return [(address, bytes(chunk)) for address, chunk in merged]


def build_uf2_blocks(segments: list[tuple[int, bytes]], family_id: int) -> bytes:
    # The Adafruit nRF52 UF2 bootloader expects every block to carry exactly
    # UF2_PAYLOAD_SIZE bytes at a UF2_PAYLOAD_SIZE-aligned target address.
    # Misaligned or short blocks are silently rejected (the bootloader keeps
    # waiting forever instead of jumping to the freshly-flashed app).
    #
    # Flatten all segments into a single bytearray spanning the lowest-to-
    # highest address, then emit one block per UF2_PAYLOAD_SIZE-aligned
    # window. Holes between segments are filled with 0xFF (erase value).
    if not segments:
        return b""

    base = min(addr for addr, _ in segments) & ~(UF2_PAYLOAD_SIZE - 1)
    end = max(addr + len(data) for addr, data in segments)
    end = (end + UF2_PAYLOAD_SIZE - 1) & ~(UF2_PAYLOAD_SIZE - 1)

    image = bytearray(b"\xFF" * (end - base))
    for address, data in segments:
        start_offset = address - base
        image[start_offset : start_offset + len(data)] = data

    blocks: list[bytes] = []
    total_blocks = (end - base) // UF2_PAYLOAD_SIZE
    block_number = 0

    for offset in range(0, len(image), UF2_PAYLOAD_SIZE):
        payload = bytes(image[offset : offset + UF2_PAYLOAD_SIZE])
        target_addr = base + offset
        header = struct.pack(
            "<IIIIIIII",
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            UF2_FLAG_FAMILY_ID_PRESENT,
            target_addr,
            UF2_PAYLOAD_SIZE,
            block_number,
            total_blocks,
            family_id,
        )
        block = header + payload + (b"\x00" * (UF2_BLOCK_SIZE - len(header) - UF2_PAYLOAD_SIZE - 4)) + struct.pack("<I", UF2_MAGIC_END)
        blocks.append(block)
        block_number += 1

    return b"".join(blocks)


def main() -> None:
    args = parse_args()
    family_id = int(args.family_id, 0)
    segments = parse_hex_segments(args.input_hex)
    uf2 = build_uf2_blocks(segments, family_id)
    args.output_uf2.write_bytes(uf2)


if __name__ == "__main__":
    main()