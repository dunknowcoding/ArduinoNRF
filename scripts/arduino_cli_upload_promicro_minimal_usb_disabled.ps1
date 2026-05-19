#Requires -Version 5.1
<#
.SYNOPSIS
  Compile + upload MinimalUsbSmoke (promicroserialnosd, usbcdc=disabled) via arduino-cli — PS 5.1 safe (no &&).

.EXAMPLE
  .\scripts\arduino_cli_upload_promicro_minimal_usb_disabled.ps1 -Port COM3 -UseArduinoCiConfig
  .\scripts\arduino_cli_upload_promicro_minimal_usb_disabled.ps1 -Port COM3 -UseArduinoCiConfig -CleanBuild
#>
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [switch]$UseArduinoCiConfig,
    [switch]$CleanBuild
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $Repo

$env:NIUS_SKIP_POST_VERIFY = '1'
if ([string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS)) {
    $env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS = '12000'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_DFU_PROCESS_TIMEOUT_MS)) {
    $env:NIUS_ADAFRUIT_DFU_PROCESS_TIMEOUT_MS = '180000'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS)) {
    $env:NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS = '90000'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_POST_TOUCH_SLEEP_MS)) {
    $env:NIUS_POST_TOUCH_SLEEP_MS = '3800'
}

$ArduinoCliPrefix = @()
if ($UseArduinoCiConfig) {
    . (Join-Path $PSScriptRoot 'arduino_cli_with_repo.ps1')
    $ArduinoCliPrefix = @(Get-ArduinoCliRepoConfigArgs -EnsureJunction)
}

$fqbn = 'arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd,usbcdc=disabled,usbdesc=no_app_dfu'
$sketch = Join-Path $Repo 'examples\MinimalUsbSmoke'

$compileArgs = @($ArduinoCliPrefix + @('compile', '--fqbn', $fqbn))
if ($CleanBuild) {
    $compileArgs += '--clean'
}
$compileArgs += $sketch

Write-Host '[arduino-cli] compile' -ForegroundColor Cyan
& arduino-cli @compileArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "[arduino-cli] upload -p $Port" -ForegroundColor Cyan
& arduino-cli @ArduinoCliPrefix upload -p $Port --fqbn $fqbn -v $sketch
exit $LASTEXITCODE
