# Release Notes — v0.1.4

Bug fix on top of [v0.1.3](RELEASE_NOTES_v0.1.3.md). Same install URL:

```
https://raw.githubusercontent.com/dunknowcoding/ArduinoNRF/main/package_arduinonrf_index.json
```

If you already installed `0.1.0–0.1.3`, open Arduino IDE 2.x's
**Tools → Board → Boards Manager**, search for **ArduinoNRF**, and click
**Update**.

## Fix: upload no longer hangs at "Checking whether the board left
bootloader mode" on same-PID clones

`upload.ps1` previously always waited for a PnP transition after the
Adafruit serial DFU completed, using a Wait-ForAdafruitRuntimeTransition
helper. On the `promicroserialnosd` (and similar) clone configuration
the bootloader and the runtime share VID:PID `0x239A:0x00B3`, so the
transition Windows is supposed to observe never happens — Windows sees
the same composite identity throughout. The wait would burn through its
timeout (tens of seconds) before giving up, leaving the IDE pinned at
`Checking whether the board left bootloader mode` and the user
wondering whether to kill the process.

Fix: when `upload.ps1` detects `runtime_usb_pid == upload.usb_pid` (the
same condition it already uses to skip the same-PID "is the COM port
already bootloader?" probe), it now also skips the post-verify wait and
exits 0 once the DFU streaming has completed.  Users who want the old
behaviour back can set `NIUS_FORCE_POST_VERIFY=1` in the upload
environment.

The previously documented `NIUS_SKIP_POST_VERIFY=1` env var still works
for the same effect on non-same-PID boards.

No firmware changes; pure host-side tooling fix.
