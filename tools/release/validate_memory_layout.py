from __future__ import annotations

import argparse
import re
from pathlib import Path


BOARD_KEY_RE = re.compile(r"^(?P<board>[a-z0-9_]+)\.(?P<key>.+)$")
MEMORY_RE = re.compile(
    r"^(?P<name>FLASH|RAM)\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*(?P<origin>[^,]+),\s*LENGTH\s*=\s*(?P<length>.+)$"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate boards.txt upload sizes against linker script memory layouts."
    )
    parser.add_argument(
        "--boards",
        type=Path,
        required=True,
        help="Path to boards.txt.",
    )
    return parser.parse_args()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def parse_numeric_literal(value: str) -> int:
    token = value.strip()
    if token.endswith("K"):
        return int(token[:-1], 0) * 1024
    if token.endswith("M"):
        return int(token[:-1], 0) * 1024 * 1024
    return int(token, 0)


def evaluate_length(expr: str, reserved_flash_bytes: int, app_start_bytes: int) -> int:
    parts = [part.strip() for part in expr.split("-")]
    total = parse_numeric_literal(parts[0])
    for part in parts[1:]:
        if part == "__nrf_reserved_flash_bytes":
            total -= reserved_flash_bytes
        elif part == "__nrf_app_start":
            total -= app_start_bytes
        else:
            total -= parse_numeric_literal(part)
    return total


def parse_boards(boards_path: Path) -> dict[str, dict[str, str]]:
    boards: dict[str, dict[str, str]] = {}
    for raw_line in boards_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        match = BOARD_KEY_RE.match(key)
        if not match:
            continue
        board = match.group("board")
        board_values = boards.setdefault(board, {})
        board_values[match.group("key")] = value.strip()
    return boards


def parse_linker_memory(linker_path: Path, reserved_flash_bytes: int, app_start_bytes: int) -> tuple[int, int]:
    flash_bytes: int | None = None
    ram_bytes: int | None = None
    for raw_line in linker_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        match = MEMORY_RE.match(line)
        if not match:
            continue
        memory_name = match.group("name")
        length_bytes = evaluate_length(match.group("length"), reserved_flash_bytes, app_start_bytes)
        if memory_name == "FLASH":
            flash_bytes = length_bytes
        elif memory_name == "RAM":
            ram_bytes = length_bytes
    require(flash_bytes is not None, f"FLASH memory section not found in {linker_path}")
    require(ram_bytes is not None, f"RAM memory section not found in {linker_path}")
    return flash_bytes, ram_bytes


def validate_board(platform_root: Path, board_name: str, values: dict[str, str]) -> str:
    ldscript = values.get("build.ldscript")
    variant = values.get("build.variant")
    maximum_size = values.get("upload.maximum_size")
    maximum_data_size = values.get("upload.maximum_data_size")
    reserved = values.get("build.storage_reserved_bytes")
    app_start = values.get("build.bootloader_app_start", "0")

    required_keys = {
        "build.ldscript": ldscript,
        "build.variant": variant,
        "upload.maximum_size": maximum_size,
        "upload.maximum_data_size": maximum_data_size,
        "build.storage_reserved_bytes": reserved,
    }
    for key, current_value in required_keys.items():
        require(current_value is not None, f"{board_name} is missing {key}")

    reserved_flash_bytes = int(reserved)
    app_start_bytes = int(app_start, 0)
    linker_path = platform_root / "variants" / str(variant) / str(ldscript)
    require(linker_path.is_file(), f"Missing linker script for {board_name}: {linker_path}")
    flash_bytes, ram_bytes = parse_linker_memory(linker_path, reserved_flash_bytes, app_start_bytes)

    require(
        int(maximum_size) == flash_bytes,
        f"{board_name} upload.maximum_size={maximum_size} but linker FLASH length is {flash_bytes}",
    )
    require(
        int(maximum_data_size) == ram_bytes,
        f"{board_name} upload.maximum_data_size={maximum_data_size} but linker RAM length is {ram_bytes}",
    )

    for menu_key in ("menu.storage.log.build.storage_reserved_bytes", "menu.storage.legacy.build.storage_reserved_bytes"):
        if menu_key in values:
            require(
                int(values[menu_key]) == reserved_flash_bytes,
                f"{board_name} {menu_key}={values[menu_key]} but build.storage_reserved_bytes={reserved_flash_bytes}",
            )

    return f"{board_name}: flash={flash_bytes} ram={ram_bytes} reserved={reserved_flash_bytes}"


def main() -> int:
    args = parse_args()
    boards_path = args.boards.resolve()
    require(boards_path.is_file(), f"boards.txt does not exist: {boards_path}")
    platform_root = boards_path.parent
    boards = parse_boards(boards_path)
    require(boards, f"No board entries found in {boards_path}")

    board_entries = {
        board_name: values
        for board_name, values in boards.items()
        if "build.variant" in values and "build.ldscript" in values
    }
    require(board_entries, f"No board build entries found in {boards_path}")

    summaries = []
    for board_name in sorted(board_entries):
        summaries.append(validate_board(platform_root, board_name, board_entries[board_name]))

    for summary in summaries:
        print(summary)
    print(f"validated {len(summaries)} boards")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())