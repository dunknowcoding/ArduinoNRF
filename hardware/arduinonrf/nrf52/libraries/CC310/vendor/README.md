# CC310 compatibility shim — vendoring notes

Working CC310 hardware crypto lives in the separate
**[NiusCrypto](https://github.com/dunknowcoding/ArduinoNRF-Crypto)** library.
Install NiusCrypto, run its vendoring scripts once per machine, then build
sketches that `#include <NrfCC310.h>` or `<NiusCrypto.h>`.

The in-package `libraries/CC310/` entry is a **compatibility shim** only: it
forwards to NiusCrypto (`depends=NiusCrypto`). Without NiusCrypto the sketch
fails to compile — same pattern as `libraries/Thread/` → NiusThread.

## What NiusCrypto vendors (not in this repo)

| Archive | Provides | How to obtain |
|---------|----------|---------------|
| `libnrf_cc310.a` | CRYS runtime on CryptoCell 310: SHA-256, HMAC-SHA-256, AES-CBC/CTR, ECDSA/ECDH P-256, TRNG | `python vendor/tools/import_cc310_sdk.py` (local nRF5 SDK 17.x) |
| `liboberon.a` | AES-128-GCM only (CRYS has no GCM) | `python vendor/tools/fetch_cc310.py` |

Both land under NiusCrypto's git-ignored `src/cortex-m4/` and `src/cc310/`.
Use the **soft-float / no-interrupts** variants — ArduinoNRF compiles
Cortex-M4 with the soft-float ABI.

One-shot setup from the NiusCrypto repo root:

```sh
python vendor/tools/setup_vendored.py [path-to-nRF5-SDK]
```

See NiusCrypto's [docs/VENDORING.md](https://github.com/dunknowcoding/ArduinoNRF-Crypto/blob/main/docs/VENDORING.md).

## Legacy in-package binary (deprecated)

Earlier drafts linked Nordic's older `libcc_310.a` directly from this folder.
That path is **deprecated**; do not copy binaries here. The shim no longer
probes for `libcc_310.a` — it requires NiusCrypto.

## License note

Nordic's CryptoCell binaries are distributed under Nordic's 5-clause license.
This repository does **not** bundle them. Each developer must accept Nordic's
terms and import/fetch locally via NiusCrypto's scripts.
