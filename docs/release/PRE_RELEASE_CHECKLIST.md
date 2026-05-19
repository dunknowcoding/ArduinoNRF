# Pre-Release Checklist

Use this checklist before publishing a real hosted board package.

## Package metadata

- [ ] `package_arduinonrf_index.json` uses real HTTPS URLs, not `example.invalid` placeholders.
- [ ] `archiveFileName`, `size`, and `checksum` match the final uploaded archive.
- [ ] Package-level `websiteURL` and `help.online` point at real release documentation.
- [ ] Platform-level `help.online` points at real platform documentation.

## Release artifacts

- [ ] Build the archive with `tools/release/build_platform_archive.py`.
- [ ] Update metadata with `tools/release/update_package_index.py`.
- [ ] Validate the final index with `tools/release/validate_package_index.py` without `--allow-placeholder-urls`.
- [ ] Validate `boards.txt` memory limits with `tools/release/validate_memory_layout.py`.
- [ ] Confirm the hosted `package_arduinonrf_index.json` is reachable over HTTPS.
- [ ] Confirm the hosted archive zip is reachable over HTTPS.

## Arduino IDE install flow

- [ ] Arduino IDE 2.x install succeeds.
- [ ] Arduino IDE 1.x install succeeds.
- [ ] Verify tool dependencies download automatically after package installation.
- [ ] Verify the board menus render correctly.
