#Requires -Version 5.1
<#
.SYNOPSIS
  Two-stage reset-cause diagnosis for the no-SWD ProMicro/nice!nano clone.

.DESCRIPTION
  Stage 1 uploads MinimalUsbSmoke, which currently falls back to bootloader.
  Instrumented firmware writes a retained SRAM cause at 0x20004004 before any
  intentional bootloader reset path.

  Stage 2 uploads ResetCauseReporter. The reporter reads 0x20004004 in
  .preinit_array and waits a cause-specific number of seconds before returning
  to serial DFU. The measured delay identifies the previous reset path.
#>
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [ValidateSet('promicroserialnosd')]
    [string]$BootloaderMenu = 'promicroserialnosd',
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$targetSketch = Join-Path $repoRoot 'examples\MinimalUsbSmoke'
$reporterSketch = Join-Path $repoRoot 'examples\ResetCauseReporter'
$fqbn = "arduinonrf:nrf52:promicro_nrf52840:bootloader=$BootloaderMenu,usbcdc=disabled,usbdesc=no_app_dfu"

function Test-PortPresent {
    param([string]$Name)
    $ports = arduino-cli board list 2>$null | Out-String
    return $ports -match [regex]::Escape($Name)
}

function Wait-PortPresent {
    param([string]$Name, [int]$Seconds)
    for ($i = 0; $i -lt $Seconds; $i++) {
        if (Test-PortPresent $Name) {
            return $true
        }
        Start-Sleep -Seconds 1
    }
    return $false
}

function Wait-PortAbsent {
    param([string]$Name, [int]$Seconds)
    for ($i = 0; $i -lt $Seconds; $i++) {
        if (-not (Test-PortPresent $Name)) {
            return $true
        }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

function Decode-Delay {
    param([double]$Seconds)
    $rounded = [int][Math]::Round($Seconds)
    if ($rounded -ge 18 -and $rounded -le 22) { return 'WDT IRQ observed' }
    if ($rounded -ge 16 -and $rounded -lt 18) { return 'HardFault handler' }
    if ($rounded -ge 14 -and $rounded -lt 16) { return 'Default_Handler / unexpected IRQ' }
    if ($rounded -ge 12 -and $rounded -lt 14) { return 'Application DFU_DETACH request' }
    if ($rounded -ge 10 -and $rounded -lt 12) { return '1200-bps DTR-drop upload reset' }
    if ($rounded -ge 8 -and $rounded -lt 10) { return 'IRQ detachRequested path' }
    if ($rounded -ge 6 -and $rounded -lt 8) { return 'poll() detachRequested path' }
    if ($rounded -ge 4 -and $rounded -lt 6) { return 'USBD config timeout reset' }
    if ($rounded -ge 2 -and $rounded -lt 4) { return 'No retained cause / unknown' }
    if ($rounded -ge 20) { return "USBD diagnostic stage $($rounded - 20)" }
    return 'Unclassified delay'
}

if (-not (Test-Path $targetSketch)) { throw "Sketch not found: $targetSketch" }
if (-not (Test-Path $reporterSketch)) { throw "Sketch not found: $reporterSketch" }

$env:NIUS_SKIP_POST_VERIFY = '1'

Write-Host "[reset-cause] fqbn=$fqbn" -ForegroundColor Cyan
if (-not (Wait-PortPresent $Port $TimeoutSeconds)) {
    throw "Port $Port did not appear before target upload"
}

Write-Host '[reset-cause] compile target MinimalUsbSmoke' -ForegroundColor Cyan
arduino-cli compile --fqbn $fqbn --clean $targetSketch
if ($LASTEXITCODE -ne 0) { throw "target compile failed exit=$LASTEXITCODE" }

Write-Host '[reset-cause] upload target MinimalUsbSmoke' -ForegroundColor Cyan
arduino-cli upload -p $Port --fqbn $fqbn -v $targetSketch
if ($LASTEXITCODE -ne 0) { throw "target upload failed exit=$LASTEXITCODE" }

Write-Host '[reset-cause] waiting for target to return to bootloader' -ForegroundColor Cyan
if (-not (Wait-PortPresent $Port $TimeoutSeconds)) {
    throw "Target did not return to $Port within $TimeoutSeconds seconds"
}

Write-Host '[reset-cause] compile reporter' -ForegroundColor Cyan
arduino-cli compile --fqbn $fqbn --clean $reporterSketch
if ($LASTEXITCODE -ne 0) { throw "reporter compile failed exit=$LASTEXITCODE" }

Write-Host '[reset-cause] upload reporter' -ForegroundColor Cyan
arduino-cli upload -p $Port --fqbn $fqbn -v $reporterSketch
if ($LASTEXITCODE -ne 0) { throw "reporter upload failed exit=$LASTEXITCODE" }

[void](Wait-PortAbsent $Port 10)
$watch = [System.Diagnostics.Stopwatch]::StartNew()
if (-not (Wait-PortPresent $Port $TimeoutSeconds)) {
    throw "Reporter did not return to $Port within $TimeoutSeconds seconds"
}
$watch.Stop()
$seconds = $watch.Elapsed.TotalSeconds

Write-Host ('[reset-cause] reporter delay: {0:N1}s' -f $seconds) -ForegroundColor Green
Write-Host ('[reset-cause] decoded cause: {0}' -f (Decode-Delay $seconds)) -ForegroundColor Green
Write-Host '[reset-cause] final board list:' -ForegroundColor Cyan
arduino-cli board list
