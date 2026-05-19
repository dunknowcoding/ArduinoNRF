from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


HTTPS_URL_RE = re.compile(r"^https://", re.IGNORECASE)
PLACEHOLDER_HOST_RE = re.compile(r"example\.invalid", re.IGNORECASE)
EXPECTED_TOOL_DEPENDENCIES = [
    ("arduino", "arm-none-eabi-gcc", "7-2017q4"),
    ("arduino", "dfu-util", "0.10.0-arduino1"),
    ("arduino", "openocd", "0.11.0-arduino2"),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate the arduinonrf Arduino Board Manager package index."
    )
    parser.add_argument(
        "--index",
        type=Path,
        required=True,
        help="Path to package_arduinonrf_index.json.",
    )
    parser.add_argument(
        "--allow-placeholder-urls",
        action="store_true",
        help="Allow example.invalid placeholder URLs during local validation.",
    )
    return parser.parse_args()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def validate_https_url(value: str, field_name: str, allow_placeholder_urls: bool) -> None:
    require(bool(value), f"{field_name} must not be empty")
    require(HTTPS_URL_RE.match(value) is not None, f"{field_name} must use https")
    if not allow_placeholder_urls:
        require(
            PLACEHOLDER_HOST_RE.search(value) is None,
            f"{field_name} still uses a placeholder host",
        )


def validate_platform(platform: dict, allow_placeholder_urls: bool) -> None:
    require(platform.get("name"), "platform.name is required")
    require(platform.get("architecture") == "nrf52", "platform.architecture must be nrf52")
    require(platform.get("version"), "platform.version is required")
    require(platform.get("archiveFileName"), "platform.archiveFileName is required")
    require(platform.get("checksum"), "platform.checksum is required")
    require(platform.get("size"), "platform.size is required")
    validate_https_url(platform.get("url", ""), "platform.url", allow_placeholder_urls)

    boards = platform.get("boards")
    require(isinstance(boards, list) and boards, "platform.boards must be a non-empty list")
    for board in boards:
        require(board.get("name"), "each platform.boards entry must have a name")

    tools_dependencies = platform.get("toolsDependencies")
    require(
        isinstance(tools_dependencies, list) and tools_dependencies,
        "platform.toolsDependencies must be a non-empty list",
    )
    for dependency in tools_dependencies:
        require(dependency.get("packager"), "tool dependency packager is required")
        require(dependency.get("name"), "tool dependency name is required")
        require(dependency.get("version"), "tool dependency version is required")

    dependency_triplets = [
        (dependency["packager"], dependency["name"], dependency["version"])
        for dependency in tools_dependencies
    ]
    require(
        dependency_triplets == EXPECTED_TOOL_DEPENDENCIES,
        "platform.toolsDependencies must match the expected Arduino tool package names and versions",
    )

    help_section = platform.get("help") or {}
    if help_section:
        validate_https_url(help_section.get("online", ""), "platform.help.online", allow_placeholder_urls)


def validate_index(index_path: Path, allow_placeholder_urls: bool) -> None:
    data = json.loads(index_path.read_text(encoding="utf-8"))
    packages = data.get("packages")
    require(isinstance(packages, list) and len(packages) == 1, "index must contain exactly one package")

    package = packages[0]
    require(package.get("name") == "arduinonrf", "package.name must be arduinonrf")
    require(package.get("maintainer"), "package.maintainer is required")
    require(package.get("websiteURL"), "package.websiteURL is required")
    require(package.get("email"), "package.email is required")
    validate_https_url(package.get("websiteURL", ""), "package.websiteURL", allow_placeholder_urls)

    help_section = package.get("help") or {}
    if help_section:
        validate_https_url(help_section.get("online", ""), "package.help.online", allow_placeholder_urls)

    platforms = package.get("platforms")
    require(isinstance(platforms, list) and platforms, "package.platforms must be a non-empty list")
    for platform in platforms:
        validate_platform(platform, allow_placeholder_urls)

    tools = package.get("tools") or []
    require(isinstance(tools, list), "package.tools must be a list when present")
    for tool in tools:
        require(tool.get("name"), "tool.name is required")
        require(tool.get("version"), "tool.version is required")
        systems = tool.get("systems")
        require(isinstance(systems, list) and systems, "tool.systems must be a non-empty list")
        for system in systems:
            require(system.get("host"), "tool system host is required")
            require(system.get("archiveFileName"), "tool system archiveFileName is required")
            require(system.get("checksum"), "tool system checksum is required")
            require(system.get("size"), "tool system size is required")
            validate_https_url(system.get("url", ""), "tool system url", allow_placeholder_urls)


def main() -> int:
    args = parse_args()
    index_path = args.index.resolve()
    require(index_path.is_file(), f"Index file does not exist: {index_path}")
    validate_index(index_path, args.allow_placeholder_urls)
    print(index_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
