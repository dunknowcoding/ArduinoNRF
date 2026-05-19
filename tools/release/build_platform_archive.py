from __future__ import annotations

import argparse
import shutil
from pathlib import Path


REQUIRED_FILES = (
    "boards.txt",
    "platform.txt",
    "programmers.txt",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build a zip archive for the arduinonrf Arduino platform."
    )
    parser.add_argument(
        "--platform-root",
        type=Path,
        required=True,
        help="Path to the platform root, for example hardware/arduinonrf/nrf52.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="Directory where the generated archive is written.",
    )
    parser.add_argument(
        "--archive-name",
        help="Base name for the generated zip archive. Defaults to arduinonrf-<platform version>.",
    )
    return parser.parse_args()


def platform_version(platform_root: Path) -> str:
    platform_txt = platform_root / "platform.txt"
    for raw_line in platform_txt.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line.startswith("version="):
            return line.split("=", 1)[1].strip()
    raise SystemExit(f"Platform version not found in {platform_txt}")


def validate_platform_root(platform_root: Path) -> None:
    missing = [name for name in REQUIRED_FILES if not (platform_root / name).is_file()]
    if missing:
        names = ", ".join(missing)
        raise SystemExit(f"Platform root is missing required files: {names}")


def build_archive(platform_root: Path, output_dir: Path, archive_name: str) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    archive_base = output_dir / archive_name
    archive_path = Path(
        shutil.make_archive(
            base_name=str(archive_base),
            format="zip",
            root_dir=str(platform_root.parent),
            base_dir=platform_root.name,
        )
    )
    return archive_path


def main() -> int:
    args = parse_args()
    platform_root = args.platform_root.resolve()
    output_dir = args.output_dir.resolve()

    if not platform_root.is_dir():
        raise SystemExit(f"Platform root does not exist: {platform_root}")

    validate_platform_root(platform_root)
    archive_name = args.archive_name or f"arduinonrf-{platform_version(platform_root)}"
    archive_path = build_archive(platform_root, output_dir, archive_name)
    print(archive_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
