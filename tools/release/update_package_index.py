from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256_for(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def join_url(base_url: str, name: str) -> str:
    return base_url.rstrip("/") + "/" + name


def optional_url(value: str | None) -> str | None:
    if value is None:
        return None
    return value.rstrip("/")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Update the ArduinoNRF board-manager index with release archive metadata."
    )
    parser.add_argument("--index", required=True, help="Path to package_arduinonrf_index.json")
    parser.add_argument("--archive", required=True, help="Path to the release zip archive")
    parser.add_argument("--download-base", required=True, help="Base URL that hosts the archive")
    parser.add_argument("--version", required=True, help="Platform version to update")
    parser.add_argument("--website-url", help="Optional package website URL to write to package.websiteURL")
    parser.add_argument("--package-help-url", help="Optional package help URL to write to package.help.online")
    parser.add_argument("--platform-help-url", help="Optional platform help URL to write to platform.help.online")
    parser.add_argument("--maintainer", help="Optional package maintainer override")
    parser.add_argument("--email", help="Optional package email override")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    index_path = Path(args.index)
    archive_path = Path(args.archive)
    archive_name = archive_path.name
    archive_size = archive_path.stat().st_size
    archive_checksum = sha256_for(archive_path)
    download_url = join_url(args.download_base, archive_name)

    data = json.loads(index_path.read_text(encoding="utf-8"))
    package = data["packages"][0]
    platform = package["platforms"][0]
    if platform["version"] != args.version:
        raise SystemExit(f"Version mismatch: index has {platform['version']}, expected {args.version}")

    platform["archiveFileName"] = archive_name
    platform["size"] = str(archive_size)
    platform["checksum"] = f"SHA-256:{archive_checksum}"
    platform["url"] = download_url

    if args.maintainer:
        package["maintainer"] = args.maintainer
    if args.email:
        package["email"] = args.email
    if args.website_url:
        package["websiteURL"] = optional_url(args.website_url)
    if args.package_help_url:
        package["help"] = {"online": optional_url(args.package_help_url)}
    if args.platform_help_url:
        platform["help"] = {"online": optional_url(args.platform_help_url)}

    index_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
