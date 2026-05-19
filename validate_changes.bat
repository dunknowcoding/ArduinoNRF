@echo off
setlocal enabledelayedexpansion

echo ========================================
echo 1. PowerShell Syntax Check (upload.ps1)
echo ========================================

REM Use PowerShell to parse and check syntax
powershell.exe -NoProfile -Command "^
try {^
    [void][System.Management.Automation.Language.Parser]::ParseFile('F:\Arduino\driver\ArduinoNRF\hardware\arduinonrf\nrf52\tools\niusrobotlab\upload.ps1', [ref]$null, [ref]$null);^
    Write-Host 'SYNTAX CHECK PASSED: upload.ps1 has no syntax errors';^
    exit 0^
} catch {^
    Write-Host 'SYNTAX ERROR: ' $_.Exception.Message;^
    exit 1^
}"

if errorlevel 1 (
    echo.
    echo VALIDATION FAILED: Syntax errors found
    exit /b 1
)

echo.
echo ========================================
echo 2. Arduino CLI Compile Test
echo ========================================

cd /d F:\Arduino\driver\ArduinoNRF

arduino-cli compile ^
  --config-file F:\Arduino\driver\ArduinoNRF\arduino-cli.local.yaml ^
  --fqbn arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd ^
  examples\MinimalUsbSmoke

if errorlevel 1 (
    echo.
    echo COMPILATION FAILED
    exit /b 1
) else (
    echo.
    echo Build succeeded
    exit /b 0
)
