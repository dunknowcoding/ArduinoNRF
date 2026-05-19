#Requires -Version 5.1
<#
.SYNOPSIS
  Fully automated compile + upload MinimalUsbSmoke for promicroserialnosd (optional COM auto-pick).

.PARAMETER Port
  COM port; if empty, uses Pick-ServiceSerialPortForGdbStub -PreferServiceCdc for promicro_nrf52840.
#>
param(
    [string]$Port = '',
    [string]$BootloaderMenu = 'promicroserialnosd',
    [switch]$MinimalUsb,
    [switch]$DualCdcUsbGdbStub,
    [switch]$UseArduinoCiConfig
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $Repo

. (Join-Path $Repo 'hardware/arduinonrf/nrf52/tools/niusrobotlab/usb_port_helpers.ps1')

if ([string]::IsNullOrWhiteSpace($Port)) {
    $Port = Pick-ServiceSerialPortForGdbStub -BoardName 'promicro_nrf52840' -PreferServiceCdc
    if (-not $Port) {
        throw 'Could not auto-pick COM (ambiguous or no board). Pass -Port COMx.'
    }
    Write-Host "[auto-upload] Auto-picked port $Port" -ForegroundColor Cyan
}

$hwSplat = @{
    Port           = $Port
    BootloaderMenu = $BootloaderMenu
}
if ($UseArduinoCiConfig) {
    $hwSplat.UseArduinoCiConfig = $true
}
if ($MinimalUsb) {
    $hwSplat.MinimalUsb = $true
}
if ($DualCdcUsbGdbStub) {
    $hwSplat.DualCdcUsbGdbStub = $true
}

Write-Host '[auto-upload] Starting hardware_upload_minimal_usb.ps1 (same process)...' -ForegroundColor Cyan
$hwScript = Join-Path $Repo 'scripts/hardware_upload_minimal_usb.ps1'
& $hwScript @hwSplat
exit $LASTEXITCODE
