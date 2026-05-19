#Requires -Version 5.1
<#
.SYNOPSIS
  Compile and upload BootReturnSmoke for app-start diagnosis.

.DESCRIPTION
  The sketch resets back to serial DFU using Adafruit's GPREGRET 0x4E magic.
  By default runtime USB is disabled; pass -RuntimeUsb to diagnose whether
  the USB bring-up path reaches setup().

.EXAMPLE
  .\scripts\hardware_upload_bootreturn_diag.ps1 -Port COM3 -AppStart 0x27000
#>
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [ValidateSet('0x1000', '0x26000', '0x27000', '0x28000')]
    [string]$AppStart = '0x26000',
    [string]$SdReq = '0x00B6',
    [ValidateSet('setup', 'preinit')]
    [string]$Mode = 'setup',
    [switch]$RuntimeUsb,
    [switch]$MinimalUsb,
    [switch]$PollOnly,
    [ValidateRange(0, 23)]
    [int]$UsbdDiagStage = 0
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$sketchFolder = if ($Mode -eq 'preinit') { 'EarlyBootReturnSmoke' } else { 'BootReturnSmoke' }
$sketch = Join-Path $repoRoot "examples\$sketchFolder"
if (-not (Test-Path $sketch)) {
    throw "Sketch not found: $sketch"
}

$packageRoot = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\arduinonrf\hardware\nrf52\0.1.0'
$uploadScript = Join-Path $packageRoot 'tools\niusrobotlab\upload.ps1'
$dfuTool = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\arduino\tools\dfu-util\0.10.0-arduino1\dfu-util.exe'

if (-not (Test-Path $uploadScript)) {
    throw "Upload script not found: $uploadScript"
}
if (-not (Test-Path $dfuTool)) {
    throw "dfu-util not found: $dfuTool"
}

$appStartValue = [Convert]::ToInt32($AppStart.Substring(2), 16)
$maximumSize = 0x100000 - $appStartValue - 0x4000 - 0x13000
$buildPath = Join-Path $repoRoot ("build\bootreturn_diag_{0}_{1}" -f $Mode, ($AppStart -replace '^0x', ''))
$boardOptions = @('bootloader=promicroserial')
if ($MinimalUsb) {
    $RuntimeUsb = $true
    $boardOptions += @('usbcdc=disabled', 'usbdesc=no_app_dfu')
}
$fqbn = 'arduinonrf:nrf52:promicro_nrf52840:' + ($boardOptions -join ',')
if (-not $RuntimeUsb) {
    $fqbn = "$fqbn,usbcdc=disabled"
}

$compileArgs = @(
    'compile',
    '--fqbn', $fqbn,
    '--build-path', $buildPath,
    '--clean',
    '--build-property', "build.bootloader_app_start=$AppStart",
    '--build-property', "upload.maximum_size=$maximumSize",
    '--build-property', 'build.bootloader_flags=-DNRF_SYSTEM_BOOTLOADER_MODE=1 -DNRF_SYSTEM_BOOTLOADER_RESET_MODE=1'
)

if (-not $RuntimeUsb) {
    $compileArgs += @(
        '--build-property', 'build.upload_mode_flags=-DNRF_SYSTEM_USB_UPLOAD_PREFERRED=0',
        '--build-property', 'build.usb_cdc_flags=-DNRF_SYSTEM_HAS_USB_CDC=0 -DNRF_SYSTEM_DEFAULT_SERIAL_USB=0'
    )
}
if ($UsbdDiagStage -gt 0) {
    $compileArgs += @('--build-property', "build.debug_flags=-DNRF_USBD_DIAG_RESET_STAGE=$UsbdDiagStage")
}
if ($RuntimeUsb -and $PollOnly) {
    $compileArgs += @('--build-property', 'build.usb_backend_flags=-DNRF_SYSTEM_USB_BACKEND=1 -DNRF_USBD_POLL_ONLY=1')
}

$compileArgs += $sketch

Write-Host "[bootreturn] compile mode=$Mode runtimeUsb=$RuntimeUsb minimalUsb=$MinimalUsb pollOnly=$PollOnly usbdDiagStage=$UsbdDiagStage appStart=$AppStart maximumSize=$maximumSize sdReq=$SdReq" -ForegroundColor Cyan
arduino-cli @compileArgs
if ($LASTEXITCODE -ne 0) {
    throw "compile failed exit=$LASTEXITCODE"
}

$job = Start-Job -ScriptBlock {
    param([int]$Seconds)
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        $ts = Get-Date -Format o
        $ports = arduino-cli board list 2>$null | Out-String
        if ($ports -match 'COM\d+|Serial Port \(USB\)') {
            "$ts BOARD $($ports -replace "`r?`n", ' | ')"
        }
        try {
            Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
                Where-Object { $_.InstanceId -match 'VID_239A|PID_00B3|PID_00B4' } |
                ForEach-Object { "$ts PNP $($_.Status) | $($_.Class) | $($_.FriendlyName) | $($_.InstanceId)" }
        } catch {
            "$ts PNP_ERROR $($_.Exception.Message)"
        }
        Start-Sleep -Milliseconds 250
    }
} -ArgumentList 25

try {
    $env:NIUS_SKIP_POST_VERIFY = '1'
    Write-Host "[bootreturn] upload $Port" -ForegroundColor Cyan
    & powershell -NoProfile -ExecutionPolicy Bypass -File $uploadScript `
        -Mode dfu `
        -Tool $dfuTool `
        -UsbVid '0x239A' `
        -UsbPid '0x00B3' `
        -Alt '0' `
        -Bin (Join-Path $buildPath "$sketchFolder.ino.bin") `
        -Hex (Join-Path $buildPath "$sketchFolder.ino.hex") `
        -Port $Port `
        -UseTouch1200 'false' `
        -WaitForUploadPort 'false' `
        -Board 'promicro_nrf52840' `
        -BootloaderMode 'adafruit-dfu' `
        -Uf2FamilyId '0xADA52840' `
        -Uf2AppStart $AppStart `
        -Uf2VolumeLabel 'NICENANO' `
        -Uf2Model 'nice!nano' `
        -Uf2BoardId 'nRF52840-nicenano' `
        -SdReq $SdReq
    if ($LASTEXITCODE -ne 0) {
        throw "upload failed exit=$LASTEXITCODE"
    }

    Start-Sleep -Seconds 12
} finally {
    Stop-Job $job -ErrorAction SilentlyContinue | Out-Null
    Write-Host '--- watcher evidence ---' -ForegroundColor Cyan
    Receive-Job $job | Select-Object -Unique
    Remove-Job $job -Force -ErrorAction SilentlyContinue
    Write-Host '--- final board list ---' -ForegroundColor Cyan
    arduino-cli board list
}
