#Requires -Version 5.1
<#
.SYNOPSIS
  Helpers so arduino-cli uses THIS repo's hardware/arduinonrf/nrf52 (same idea as usb_single_cable_ci.ps1).

.DESCRIPTION
  - Ensures junction: .arduino-ci-sketchbook/hardware/arduinonrf/nrf52 -> repo hardware/arduinonrf/nrf52
  - Returns extra arguments for arduino-cli: --config-file <repo>/.arduino-ci.yaml

  Usage (from another script):
    . (Join-Path $PSScriptRoot 'arduino_cli_with_repo.ps1')
    $extra = Get-ArduinoCliRepoConfigArgs -EnsureJunction
    & arduino-cli @extra compile --fqbn ... examples\MinimalUsbSmoke
#>

function Get-ArduinoNrfRepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}

function Ensure-ArduinoCiSketchbookJunction {
    $repoRoot = Get-ArduinoNrfRepoRoot
    $sketchbook = Join-Path $repoRoot '.arduino-ci-sketchbook'
    $hardwareVendor = Join-Path $sketchbook 'hardware\arduinonrf'
    $link = Join-Path $hardwareVendor 'nrf52'
    $target = Join-Path $repoRoot 'hardware\arduinonrf\nrf52'

    if (-not (Test-Path $target)) {
        throw "Platform not found: $target"
    }

    New-Item -ItemType Directory -Force -Path $sketchbook | Out-Null
    if (-not (Test-Path $hardwareVendor)) {
        New-Item -ItemType Directory -Force -Path $hardwareVendor | Out-Null
    }

    if (Test-Path $link) {
        $item = Get-Item $link -Force -ErrorAction SilentlyContinue
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            return
        }
        Remove-Item $link -Recurse -Force
    }

    Write-Host "[arduino-cli-repo] junction: $link -> $target" -ForegroundColor Cyan
    cmd /c "mklink /J `"$link`" `"$target`"" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "mklink /J failed for sketchbook junction (need repo on NTFS / admin if denied)."
    }
}

function Initialize-ArduinoCiYamlIfMissing {
    $repoRoot = Get-ArduinoNrfRepoRoot
    $cfg = Join-Path $repoRoot '.arduino-ci.yaml'
    $sketchbook = Join-Path $repoRoot '.arduino-ci-sketchbook'
    $userLine = $sketchbook.Replace('\', '/')

    if (-not (Test-Path $cfg)) {
        @"
directories:
  user: '$userLine'
"@ | Set-Content -Path $cfg -Encoding UTF8
        Write-Host "[arduino-cli-repo] created $cfg" -ForegroundColor Yellow
    }
}

function Get-ArduinoCliRepoConfigArgs {
    param(
        [switch]$EnsureJunction
    )

    $repoRoot = Get-ArduinoNrfRepoRoot
    $cfg = Join-Path $repoRoot '.arduino-ci.yaml'
    Initialize-ArduinoCiYamlIfMissing

    if (-not (Test-Path $cfg)) {
        throw "Missing $cfg"
    }

    if ($EnsureJunction) {
        Ensure-ArduinoCiSketchbookJunction
    }

    return @('--config-file', $cfg)
}
