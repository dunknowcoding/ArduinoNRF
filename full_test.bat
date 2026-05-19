@echo off
REM Full Arduino build, upload and monitoring sequence
REM Working directory: F:\Arduino\driver\ArduinoNRF

setlocal enabledelayedexpansion

cd /d "F:\Arduino\driver\ArduinoNRF"

echo.
echo ======================================================================
echo TASK 2: Build the sketch
echo ======================================================================
echo.
echo Command: arduino-cli compile --config-file=arduino-cli.local.yaml --fqbn=arduinonrf:nrf52:promicro_nrf52840 examples/UsbSerial
echo Building...

set BUILD_START=%TIME%
arduino-cli compile --config-file=arduino-cli.local.yaml --fqbn=arduinonrf:nrf52:promicro_nrf52840 examples/UsbSerial
set BUILD_RESULT=%ERRORLEVEL%
set BUILD_END=%TIME%

if %BUILD_RESULT% equ 0 (
    echo.
    echo ✓ Build SUCCEEDED
) else (
    echo.
    echo ✗ Build FAILED with error code %BUILD_RESULT%
    exit /b 1
)

echo.
echo ======================================================================
echo TASK 3: Upload to COM3
echo ======================================================================
echo.
echo Command: arduino-cli upload --config-file=arduino-cli.local.yaml --fqbn=arduinonrf:nrf52:promicro_nrf52840 --port=COM3 examples/UsbSerial
echo Uploading...

set UPLOAD_START=%TIME%
arduino-cli upload --config-file=arduino-cli.local.yaml --fqbn=arduinonrf:nrf52:promicro_nrf52840 --port=COM3 examples/UsbSerial
set UPLOAD_RESULT=%ERRORLEVEL%
set UPLOAD_END=%TIME%

if %UPLOAD_RESULT% equ 0 (
    echo.
    echo ✓ Upload SUCCEEDED
) else (
    echo.
    echo ✗ Upload FAILED with error code %UPLOAD_RESULT%
    exit /b 1
)

echo.
echo ======================================================================
echo TASK 4: Monitor port enumeration for 120 seconds
echo ======================================================================
echo.
echo Monitoring for new COM port enumeration...

REM Call Python to monitor ports
python "F:\Arduino\driver\ArduinoNRF\monitor_ports.py"
set MONITOR_RESULT=%ERRORLEVEL%

if %MONITOR_RESULT% equ 0 (
    echo.
    echo ✓ CDC port enumeration detected
) else (
    echo.
    echo ✗ No CDC port enumeration detected
    exit /b 1
)

echo.
echo ======================================================================
echo TASK 5: Second upload (if conditions met)
echo ======================================================================
echo.
echo Attempting second upload...

arduino-cli upload --config-file=arduino-cli.local.yaml --fqbn=arduinonrf:nrf52:promicro_nrf52840 --port=COM4 examples/UsbSerial
set SECOND_UPLOAD_RESULT=%ERRORLEVEL%

if %SECOND_UPLOAD_RESULT% equ 0 (
    echo.
    echo ✓ Second upload SUCCEEDED
) else (
    echo.
    echo ✗ Second upload FAILED
    exit /b 1
)

echo.
echo ======================================================================
echo ALL TASKS COMPLETED SUCCESSFULLY
echo ======================================================================
echo.

endlocal
