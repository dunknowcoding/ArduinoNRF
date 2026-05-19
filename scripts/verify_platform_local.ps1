#Requires -Version 5.1
<#
.SYNOPSIS
  Local regression: CI-style compiles + usb_port_helpers + arduino-cli junction + optional conda inventory.

.DESCRIPTION
  Does NOT upload firmware (avoid blocking on DFU). For board programming use:
    .\scripts\hardware_upload_minimal_usb.ps1 -Port COMx -UseArduinoCiConfig ...

.PARAMETER SkipCi
  Skip scripts/usb_single_cable_ci.ps1 (saves ~1 minute).

.PARAMETER WithIronEngineInventory
  Run conda IronEngineWorld python scripts/win_serial_inventory.py --json

.EXAMPLE
  .\scripts\verify_platform_local.ps1
  .\scripts\verify_platform_local.ps1 -SkipCi
  .\scripts\verify_platform_local.ps1 -WithIronEngineInventory
#>
param(
    [switch]$SkipCi,
    [switch]$WithIronEngineInventory
)

$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

if (-not $SkipCi) {
    Write-Host '[verify_platform_local] running usb_single_cable_ci.ps1 -SkipClone' -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'usb_single_cable_ci.ps1') -SkipClone
}

Write-Host '[verify_platform_local] verify_helpers_smoke.ps1' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'verify_helpers_smoke.ps1')

Write-Host '[verify_platform_local] verify_repo_compile_dual_cdc.ps1' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'verify_repo_compile_dual_cdc.ps1')

if ($WithIronEngineInventory) {
    Write-Host '[verify_platform_local] conda IronEngineWorld win_serial_inventory.py --json' -ForegroundColor Cyan
    conda run -n IronEngineWorld python (Join-Path $PSScriptRoot 'win_serial_inventory.py') --json
    if ($LASTEXITCODE -ne 0) {
        throw 'win_serial_inventory.py failed'
    }
}

Write-Host '[verify_platform_local] ALL OK (software gates; upload/DFU not exercised).' -ForegroundColor Green
