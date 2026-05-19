# Release Flow

This directory contains the local release helpers for the ArduinoNRF package.

## Current state

- A local Board Manager archive can be built from `hardware/arduinonrf/nrf52`.
- `update_package_index.py` can inject real archive metadata into `package_arduinonrf_index.json`.
- `validate_package_index.py` can validate the index shape, with optional placeholder URL allowance for local dry runs.
- `validate_memory_layout.py` can verify that `boards.txt` sketch and RAM limits match the active linker scripts and reserved flash settings.
- `compile_matrix.ps1` covers representative native-USB and SWD-first boards plus the current smoke sketches.

## Local release sequence

### 1. Build the archive

```powershell
f:/Arduino/driver/ArduinoNRF/.venv/Scripts/python.exe tools/release/build_platform_archive.py --platform-root hardware/arduinonrf/nrf52 --output-dir dist
```

### 2. Update the package index

```powershell
f:/Arduino/driver/ArduinoNRF/.venv/Scripts/python.exe tools/release/update_package_index.py --index package_arduinonrf_index.json --archive dist/arduinonrf-0.1.0.zip --download-base https://example.invalid/arduinonrf --version 0.1.0 --maintainer NiusRobotLab
```

### 3. Validate the package index

```powershell
f:/Arduino/driver/ArduinoNRF/.venv/Scripts/python.exe tools/release/validate_package_index.py --index package_arduinonrf_index.json --allow-placeholder-urls
```

### 4. Validate board memory limits

```powershell
python tools/release/validate_memory_layout.py --boards hardware/arduinonrf/nrf52/boards.txt
```

### 5. Run the compile matrix

```powershell
powershell -ExecutionPolicy Bypass -File tools/release/compile_matrix.ps1
```
