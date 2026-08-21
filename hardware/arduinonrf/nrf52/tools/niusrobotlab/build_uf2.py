from __future__ import annotations

import argparse
import struct
import sys
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
    parser.add_argument("--output-uf2", type=Path)
    parser.add_argument("--family-id", required=True)
    parser.add_argument(
        "--app-start",
        help="required application vector address; must be paired with --max-size",
    )
    parser.add_argument(
        "--max-size",
        help="maximum application bytes from --app-start; must be paired with --app-start",
    )
    parser.add_argument(
        "--ram-end",
        help="exclusive upper SRAM address for vector validation; required with --app-start",
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="validate Intel HEX and application layout without writing UF2",
    )
    return parser.parse_args()


def parse_hex_segments(path: Path) -> list[tuple[int, bytes]]:
    upper = 0
    segments: list[tuple[int, bytes]] = []
    saw_eof = False

    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line:
            continue
        if saw_eof:
            raise ValueError(f"Data after Intel HEX EOF at line {line_number}")
        if not line.startswith(":"):
            raise ValueError(f"Invalid Intel HEX record at line {line_number}")

        try:
            record = bytes.fromhex(line[1:])
        except ValueError as error:
            raise ValueError(
                f"Invalid Intel HEX encoding at line {line_number}"
            ) from error
        if len(record) < 5:
            raise ValueError(f"Truncated Intel HEX record at line {line_number}")
        length = record[0]
        if len(record) != length + 5:
            raise ValueError(f"Intel HEX length mismatch at line {line_number}")
        if sum(record) & 0xFF:
            raise ValueError(f"Intel HEX checksum mismatch at line {line_number}")
        address = (record[1] << 8) | record[2]
        record_type = record[3]
        data = record[4 : 4 + length]

        if record_type == 0x00:
            if length == 0:
                raise ValueError(f"Empty Intel HEX data record at line {line_number}")
            absolute = upper + address
            if absolute > 0xFFFF_FFFF or absolute + length > 0x1_0000_0000:
                raise ValueError(f"Intel HEX address overflow at line {line_number}")
            segments.append((absolute, data))
        elif record_type == 0x01:
            if length != 0 or address != 0:
                raise ValueError(f"Malformed Intel HEX EOF at line {line_number}")
            saw_eof = True
        elif record_type == 0x02:
            # Extended Segment Address: base = (16-bit segment) << 4. Used by
            # arm-none-eabi-objcopy when the load address fits in 20 bits, e.g.
            # 0x26000 on the ProMicro. Without this the absolute address is
            # truncated and the firmware is written to the wrong flash location.
            if length != 2 or address != 0:
                raise ValueError(
                    f"Malformed Intel HEX segment address at line {line_number}"
                )
            upper = int.from_bytes(data, "big") << 4
        elif record_type == 0x04:
            if length != 2 or address != 0:
                raise ValueError(
                    f"Malformed Intel HEX linear address at line {line_number}"
                )
            upper = int.from_bytes(data, "big") << 16
        elif record_type in (0x03, 0x05):
            if length != 4 or address != 0:
                raise ValueError(
                    f"Malformed Intel HEX start address at line {line_number}"
                )
        else:
            raise ValueError(
                f"Unsupported Intel HEX record type 0x{record_type:02X} "
                f"at line {line_number}"
            )

    if not saw_eof:
        raise ValueError(f"Intel HEX EOF record missing from {path}")

    if not segments:
        raise ValueError(f"No data records found in {path}")

    segments.sort(key=lambda item: item[0])
    merged: list[tuple[int, bytearray]] = []
    for address, chunk in segments:
        if merged:
            previous_end = merged[-1][0] + len(merged[-1][1])
            if address < previous_end:
                raise ValueError(
                    f"Overlapping Intel HEX data at 0x{address:08X}"
                )
            if address == previous_end:
                merged[-1][1].extend(chunk)
                continue
        merged.append((address, bytearray(chunk)))

    return [(address, bytes(chunk)) for address, chunk in merged]


def _bytes_at(
    segments: list[tuple[int, bytes]], address: int, length: int
) -> bytes:
    result = bytearray()
    for cursor in range(address, address + length):
        value = None
        for segment_address, data in segments:
            if segment_address <= cursor < segment_address + len(data):
                value = data[cursor - segment_address]
                break
        if value is None:
            raise ValueError(
                f"Application vector table has a hole at 0x{cursor:08X}"
            )
        result.append(value)
    return bytes(result)


def validate_application_layout(
    segments: list[tuple[int, bytes]], app_start: int, max_size: int, ram_end: int
) -> None:
    """Reject an image that cannot be the selected nRF52 application layout."""
    if not 0 <= app_start <= 0xFFFF_FFFF:
        raise ValueError("Application start is outside the 32-bit address space")
    if max_size <= 0 or app_start + max_size > 0x1_0000_0000:
        raise ValueError("Application maximum size is invalid")
    if not 0x2000_0000 < ram_end <= 0x2004_0000:
        raise ValueError("Application SRAM end is invalid for a supported nRF52 target")

    image_start = min(address for address, _ in segments)
    image_end = max(address + len(data) for address, data in segments)
    allowed_end = app_start + max_size
    if image_start != app_start:
        raise ValueError(
            f"Application image starts at 0x{image_start:08X}; "
            f"selected layout requires 0x{app_start:08X}"
        )
    if image_end > allowed_end:
        raise ValueError(
            f"Application image ends at 0x{image_end:08X}; "
            f"selected layout ends at 0x{allowed_end:08X}"
        )

    vector = _bytes_at(segments, app_start, 8)
    stack_pointer, reset_vector = struct.unpack("<II", vector)
    if not 0x2000_0000 < stack_pointer <= ram_end or stack_pointer & 0x7:
        raise ValueError(
            f"Initial stack pointer 0x{stack_pointer:08X} exceeds the selected "
            f"target SRAM ending at 0x{ram_end:08X}"
        )
    if reset_vector & 1 == 0:
        raise ValueError(
            f"Reset vector 0x{reset_vector:08X} is not a Thumb entry"
        )
    reset_address = reset_vector & ~1
    if not app_start <= reset_address < image_end:
        raise ValueError(
            f"Reset vector 0x{reset_vector:08X} is outside the application image"
        )
    if not any(
        address <= reset_address < address + len(data)
        for address, data in segments
    ):
        raise ValueError(
            f"Reset vector 0x{reset_vector:08X} points into an image hole"
        )


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
    if args.validate_only == (args.output_uf2 is not None):
        raise ValueError("select exactly one of --output-uf2 or --validate-only")
    family_id = int(args.family_id, 0)
    if not 0 <= family_id <= 0xFFFF_FFFF:
        raise ValueError("UF2 family ID is outside the 32-bit range")
    segments = parse_hex_segments(args.input_hex)
    layout_values = (args.app_start, args.max_size, args.ram_end)
    if any(value is not None for value in layout_values) and not all(
        value is not None for value in layout_values
    ):
        raise ValueError("--app-start, --max-size, and --ram-end must be supplied together")
    if args.app_start is not None:
        validate_application_layout(
            segments,
            int(args.app_start, 0),
            int(args.max_size, 0),
            int(args.ram_end, 0),
        )
    if not args.validate_only:
        uf2 = build_uf2_blocks(segments, family_id)
        args.output_uf2.write_bytes(uf2)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"[nius-uf2] {error}", file=sys.stderr)
        raise SystemExit(2) from None
