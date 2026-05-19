from __future__ import annotations

import argparse
import json
from pathlib import Path

import build_platform_archive
import update_package_index
import validate_package_index


def load_config(config_path: Path | None) -> dict:
    if config_path is None:
        return {}
    if not config_path.is_file():
        raise SystemExit(f"Release config does not exist: {config_path}")
    return json.loads(config_path.read_text(encoding="utf-8"))


def optional_string(value: object) -> str | None:
    if value is None:
        return None
    text = str(value).strip()
    return text or None


def resolve_value(cli_value: object, config: dict, key: str, default: object = None) -> object:
    if cli_value not in (None, ""):
        return cli_value
    if key in config and config[key] not in (None, ""):
        return config[key]
    return default


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build a release archive, write a publish-ready package index copy, and validate it."
    )
    parser.add_argument("--config", type=Path, help="Optional JSON config file for release values.")
    parser.add_argument(
        "--platform-root",
        type=Path,
        default=Path("hardware/arduinonrf/nrf52"),
        help="Path to the platform root.",
    )
    parser.add_argument(
        "--index",
        type=Path,
        default=Path("package_arduinonrf_index.json"),
        help="Source package index template.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("dist/release"),
        help="Directory where the prepared archive and package index copy are written.",
    )
    parser.add_argument("--archive-name", help="Optional archive base name override.")
    parser.add_argument("--version", help="Optional platform version override.")
    parser.add_argument("--download-base", help="HTTPS base URL that hosts the archive.")
    parser.add_argument("--website-url", help="Package website URL.")
    parser.add_argument("--package-help-url", help="Package help URL.")
    parser.add_argument("--platform-help-url", help="Platform help URL.")
    parser.add_argument("--maintainer", help="Package maintainer.")
    parser.add_argument("--email", help="Package email.")
    parser.add_argument(
        "--allow-placeholder-urls",
        action="store_true",
        help="Allow example.invalid placeholders for local dry runs.",
    )
    return parser.parse_args()


def update_index_copy(
    source_index: Path,
    output_index: Path,
    archive_path: Path,
    *,
    version: str,
    download_base: str,
    website_url: str | None,
    package_help_url: str | None,
    platform_help_url: str | None,
    maintainer: str | None,
    email: str | None,
) -> None:
    data = json.loads(source_index.read_text(encoding="utf-8"))
    package = data["packages"][0]
    platform = package["platforms"][0]

    if platform["version"] != version:
        raise SystemExit(
            f"Version mismatch: index has {platform['version']}, expected {version}"
        )

    platform["archiveFileName"] = archive_path.name
    platform["size"] = str(archive_path.stat().st_size)
    platform["checksum"] = f"SHA-256:{update_package_index.sha256_for(archive_path)}"
    platform["url"] = update_package_index.join_url(download_base, archive_path.name)

    if maintainer:
        package["maintainer"] = maintainer
    if email:
        package["email"] = email
    if website_url:
        package["websiteURL"] = update_package_index.optional_url(website_url)
    if package_help_url:
        package["help"] = {"online": update_package_index.optional_url(package_help_url)}
    if platform_help_url:
        platform["help"] = {"online": update_package_index.optional_url(platform_help_url)}

    output_index.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    config = load_config(args.config.resolve() if args.config else None)

    platform_root = Path(resolve_value(args.platform_root, config, "platform_root", Path("hardware/arduinonrf/nrf52"))).resolve()
    index_path = Path(resolve_value(args.index, config, "index", Path("package_arduinonrf_index.json"))).resolve()
    output_dir = Path(resolve_value(args.output_dir, config, "output_dir", Path("dist/release"))).resolve()

    build_platform_archive.validate_platform_root(platform_root)
    version = str(resolve_value(args.version, config, "version", build_platform_archive.platform_version(platform_root)))
    archive_name = str(resolve_value(args.archive_name, config, "archive_name", f"arduinonrf-{version}"))
    download_base = optional_string(resolve_value(args.download_base, config, "download_base"))
    if not download_base:
        raise SystemExit("A download base URL is required. Provide --download-base or a config value.")

    website_url = optional_string(resolve_value(args.website_url, config, "website_url"))
    package_help_url = optional_string(resolve_value(args.package_help_url, config, "package_help_url"))
    platform_help_url = optional_string(resolve_value(args.platform_help_url, config, "platform_help_url"))
    maintainer = optional_string(resolve_value(args.maintainer, config, "maintainer"))
    email = optional_string(resolve_value(args.email, config, "email"))
    allow_placeholder_urls = bool(resolve_value(args.allow_placeholder_urls, config, "allow_placeholder_urls", False))

    output_dir.mkdir(parents=True, exist_ok=True)
    archive_path = build_platform_archive.build_archive(platform_root, output_dir, archive_name)
    output_index = output_dir / index_path.name
    update_index_copy(
        index_path,
        output_index,
        archive_path,
        version=version,
        download_base=download_base,
        website_url=website_url,
        package_help_url=package_help_url,
        platform_help_url=platform_help_url,
        maintainer=maintainer,
        email=email,
    )
    validate_package_index.validate_index(output_index, allow_placeholder_urls)
    print(output_index)
    print(archive_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())