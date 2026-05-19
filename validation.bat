@echo off
REM Arduino nRF52840 Validation Script
REM Requires: Python with pyserial, arduino-cli

setlocal enabledelayedexpansion

cd /d "f:\Arduino\driver\ArduinoNRF"

echo.
echo ========================================================================
echo            ARDUINO nRF52840 ProMicro VALIDATION SCRIPT
echo ========================================================================
echo.

REM TASK 1: Check COM ports
echo TASK 1: COM PORTS AND DEVICE INFO
echo -----------------------------------------------------------------------
python -c ^
"import serial.tools.list_ports; ^
ports = list(serial.tools.list_ports.comports()); ^
print(f'Found {len(ports)} COM ports:'); ^
[print(f'  {p.device}: {p.description} (VID:PID={p.vid:04X}:{p.pid:04X})' if p.vid else f'  {p.device}: {p.description}') for p in ports]; ^
com3 = [p for p in ports if p.device == 'COM3']; ^
print(f'COM3 Present: {\"YES\" if com3 else \"NO\"}'); ^
[print(f'  VID:PID: {com3[0].vid:04X}:{com3[0].pid:04X}') for p in com3 if p.vid]"

echo.
echo TASK 2: BUILD SKETCH
echo -----------------------------------------------------------------------
echo Building: examples\UsbSerial
echo FQBN: arduinonrf:nrf52:promicro_nrf52840
echo.

set BUILD_START=%time%
arduino-cli compile --config-file=arduino-cli.local.yaml --fqbn=arduinonrf:nrf52:promicro_nrf52840 examples/UsbSerial
set BUILD_STATUS=%errorlevel%
set BUILD_END=%time%

if %BUILD_STATUS% EQU 0 (
    echo.
    echo Build Status: SUCCESS
    if exist "build\arduinonrf_nrf52_promicro_nrf52840\UsbSerial.ino.bin" (
        echo Binary created successfully
        for %%A in ("build\arduinonrf_nrf52_promicro_nrf52840\UsbSerial.ino.bin") do (
            echo Size: %%~zA bytes
        )
    )
) else (
    echo.
    echo Build Status: FAILED (error level: %BUILD_STATUS%)
    goto :EOF
)

echo.
echo TASK 3: UPLOAD TO COM3
echo -----------------------------------------------------------------------
echo Target Port: COM3
echo Bootloader Mode (expecting VID:PID 239A:00B3)
echo.

set UPLOAD_START=%time%
arduino-cli upload --config-file=arduino-cli.local.yaml --fqbn=arduinonrf:nrf52:promicro_nrf52840 --port=COM3 examples/UsbSerial
set UPLOAD_STATUS=%errorlevel%
set UPLOAD_END=%time%

if %UPLOAD_STATUS% EQU 0 (
    echo.
    echo Upload Status: SUCCESS
) else (
    echo.
    echo Upload Status: FAILED (error level: %UPLOAD_STATUS%)
    goto :EOF
)

echo.
echo TASK 4: MONITOR CDC ENUMERATION
echo -----------------------------------------------------------------------
echo Monitoring for new COM ports (120 seconds)...
echo.

python << PYTHON_EOF
import serial.tools.list_ports
import time
import sys

ports_before = set([p.device for p in serial.tools.list_ports.comports()])
print(f"Initial ports: {sorted(ports_before)}")

start_time = time.time()
found_port = False
found_time = 0

while time.time() - start_time < 120:
    ports_now = set([p.device for p in serial.tools.list_ports.comports()])
    new_ports = ports_now - ports_before
    
    if new_ports:
        found_time = time.time() - start_time
        new_port = sorted(list(new_ports))[0]
        print(f"\nCDC Enumeration detected!")
        print(f"  Time: {found_time:.2f} seconds")
        print(f"  New Port: {new_port}")
        
        # Get info on new port
        for p in serial.tools.list_ports.comports():
            if p.device == new_port:
                print(f"  Description: {p.description}")
                if p.vid:
                    print(f"  VID:PID: {p.vid:04X}:{p.pid:04X}")
        
        found_port = True
        break
    
    elapsed = time.time() - start_time
    if int(elapsed) % 10 == 0 and elapsed > 0:
        sys.stdout.write(f"  {int(elapsed)}s...")
        sys.stdout.flush()
    
    time.sleep(0.5)

if not found_port:
    elapsed = time.time() - start_time
    print(f"\nTimeout: No new port detected after {elapsed:.2f} seconds")
    ports_final = set([p.device for p in serial.tools.list_ports.comports()])
    print(f"Current ports: {sorted(ports_final)}")

PYTHON_EOF

echo.
echo TASK 5: FINAL PORT STATUS
echo -----------------------------------------------------------------------

python -c ^
"import serial.tools.list_ports; ^
ports = list(serial.tools.list_ports.comports()); ^
print('Final COM ports:'); ^
[print(f'  {p.device}: {p.description} (VID:PID={p.vid:04X}:{p.pid:04X})' if p.vid else f'  {p.device}: {p.description}') for p in ports]"

echo.
echo ========================================================================
echo                    VALIDATION COMPLETE
echo ========================================================================
echo.
echo Summary:
echo   Build Status: %BUILD_STATUS% (0=success)
echo   Upload Status: %UPLOAD_STATUS% (0=success)
echo.
echo Next steps:
echo   1. Check for new COM port above
echo   2. Open new COM port at 115200 baud
echo   3. Verify "tick" messages appear every 250ms
echo.
endlocal
