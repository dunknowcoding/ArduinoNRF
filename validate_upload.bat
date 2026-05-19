@echo off
setlocal enabledelayedexpansion

REM Validate upload.ps1 and run compile
echo Validating upload.ps1 syntax...

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "^
$e = @(); ^
[System.Management.Automation.Language.Parser]::ParseFile('F:\Arduino\driver\ArduinoNRF\hardware\arduinonrf\nrf52\tools\niusrobotlab\upload.ps1', [ref]@(), [ref]$e); ^
if ($e.Count -eq 0) { Write-Host 'PASS: upload.ps1 - No syntax errors'; exit 0 } else { Write-Host 'FAIL: Syntax errors found'; exit 1 }^
"

if errorlevel 1 (
    echo 1^) upload.ps1 Syntax Check: FAILED
    exit /b 1
)

echo 1^) upload.ps1 Syntax Check: PASSED

echo.
echo Running arduino-cli compile...
cd /d F:\Arduino\driver\ArduinoNRF

arduino-cli compile --config-file F:\Arduino\driver\ArduinoNRF\arduino-cli.local.yaml --fqbn arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd F:\Arduino\driver\ArduinoNRF\examples\MinimalUsbSmoke

if errorlevel 1 (
    echo 2^) arduino-cli compile: FAILED
    exit /b 1
) else (
    echo 2^) arduino-cli compile: SUCCESS
    exit /b 0
)
