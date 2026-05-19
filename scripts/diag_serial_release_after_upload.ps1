#Requires -Version 5.1
<#
.SYNOPSIS
  Optional: flash MinimalUsb (-MinimalUsb), then probe until System.IO.Ports.SerialPort can Open/Close exclusively.

.EXAMPLE
  .\scripts\diag_serial_release_after_upload.ps1 -Port COM3 -FlashOnce -UseArduinoCiConfig
  .\scripts\diag_serial_release_after_upload.ps1 -Port COM3 -PollSeconds 120
#>
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [switch]$FlashOnce,
    [switch]$UseArduinoCiConfig,
    [int]$PollSeconds = 120,
    [int]$IntervalMs = 250
)

$ErrorActionPreference = 'Continue'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $Repo

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

$tAfterMark = Get-Date

if ($FlashOnce) {
    $hw = Join-Path $PSScriptRoot 'hardware_upload_minimal_usb.ps1'
    $splat = @{
        Port           = $Port.Trim()
        BootloaderMenu = 'promicroserialnosd'
        MinimalUsb     = $true
    }
    if ($UseArduinoCiConfig) {
        $splat.UseArduinoCiConfig = $true
    }
    Write-Host '[diag] FlashOnce: hardware_upload_minimal_usb.ps1 (-MinimalUsb)...' -ForegroundColor Cyan
    & $hw @splat
    if ($LASTEXITCODE -ne 0) {
        Write-Host ("[diag] Flash failed LASTEXITCODE={0}" -f $LASTEXITCODE) -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

$tAfterMark = Get-Date
Write-Host ("[diag] Polling exclusive Open on {0} for up to {1}s (every {2}ms)..." -f $Port, $PollSeconds, $IntervalMs) -ForegroundColor Cyan

$deadline = $tAfterMark.AddSeconds($PollSeconds)
$attempt = 0
$lastErr = ''
while ((Get-Date) -lt $deadline) {
    $attempt++
    $serial = $null
    try {
        $serial = New-Object System.IO.Ports.SerialPort $Port.Trim(), 115200, 'None', 8, 'One'
        $serial.DtrEnable = $false
        $serial.RtsEnable = $false
        $serial.ReadTimeout = 100
        $serial.WriteTimeout = 100
        $serial.Open()
        $serial.Close()
        $elapsedMs = [int](((Get-Date) - $tAfterMark).TotalMilliseconds)
        Write-Host ("[diag] OK: exclusive open succeeded after {0} ms (attempt {1})." -f $elapsedMs, $attempt) -ForegroundColor Green
        exit 0
    }
    catch {
        $lastErr = $_.Exception.Message
        if ($attempt -eq 1 -or ($attempt % 20 -eq 0)) {
            Write-Host ("[diag] attempt {0}: {1}" -f $attempt, $lastErr) -ForegroundColor DarkYellow
        }
    }
    finally {
        if ($null -ne $serial) {
            try {
                $serial.Dispose()
            }
            catch {
            }
        }
    }
    Start-Sleep -Milliseconds $IntervalMs
}

Write-Host ("[diag] FAIL: still cannot open after {0}s. Last error: {1}" -f $PollSeconds, $lastErr) -ForegroundColor Red
exit 1
