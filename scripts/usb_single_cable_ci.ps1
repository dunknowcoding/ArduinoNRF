#Requires -Version 5.1
<#
.SYNOPSIS
  Automated checks for docs/USB_SINGLE_CABLE_PLAN.md (compile matrix + ELF layout).

.DESCRIPTION
  - Uses a sketchbook junction so `arduino-cli` builds against THIS repo under hardware/arduinonrf/nrf52.
  - Compiles examples/MinimalUsbSmoke for multiple board-menu combinations (Phase B/C/D automation).
  - Verifies `.isr_vector` VMA matches expected application flash origin (0x1000 / 0x26000 / 0x27000).
  - Optionally shallow-clones reference repos into `.external/` (Phase A reference mirror).

  Hardware enumeration (USB COM / UF2) cannot run headless here — see report section [HW_REQUIRED].

.PARAMETER SkipClone
  Skip git clone of external reference repositories.

.PARAMETER RepoRoot
  Repository root (parent of hardware/ and scripts/).
#>
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [switch]$SkipClone
)

$ErrorActionPreference = 'Stop'

function Write-Step([string]$Msg) {
    Write-Host "[usb-ci] $Msg" -ForegroundColor Cyan
}

function Ensure-Junction([string]$Link, [string]$Target) {
    if (Test-Path $Link) {
        $item = Get-Item $Link -Force -ErrorAction SilentlyContinue
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            return
        }
        Remove-Item $Link -Recurse -Force
    }
    $parent = Split-Path $Link -Parent
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Write-Step "junction: $Link -> $Target"
    cmd /c "mklink /J `"$Link`" `"$Target`"" | Out-Null
}

function Get-CompilerBin([string]$ConfigFile, [string]$SketchPath, [string]$Fqbn) {
    $raw = & arduino-cli compile --config-file $ConfigFile --fqbn $Fqbn --show-properties $SketchPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "arduino-cli --show-properties failed: $raw"
    }
    $line = $raw | Where-Object { $_ -match '^compiler\.path=' } | Select-Object -First 1
    if (-not $line) { throw 'compiler.path not found in show-properties' }
    $p = ($line -replace '^compiler\.path=', '').Trim()
    if (-not $p) { throw 'empty compiler.path' }
    return $p
}

function Get-IsrVectorVma([string]$Objdump, [string]$ElfPath) {
    $lines = & $Objdump -h $ElfPath 2>&1
    if ($LASTEXITCODE -ne 0) { throw "objdump failed for $ElfPath" }
    foreach ($line in $lines) {
        if ($line -match '\.isr_vector\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+') {
            return [Convert]::ToUInt32($Matches[1], 16)
        }
    }
    throw ".isr_vector section not found in $ElfPath"
}

$Sketchbook = Join-Path $RepoRoot '.arduino-ci-sketchbook'
$ConfigFile = Join-Path $RepoRoot '.arduino-ci.yaml'
$SketchPath = Join-Path $RepoRoot 'examples\MinimalUsbSmoke'
$HardwareVendor = Join-Path $Sketchbook 'hardware\arduinonrf'
$PlatformLink = Join-Path $HardwareVendor 'nrf52'
$PlatformTarget = Join-Path $RepoRoot 'hardware\arduinonrf\nrf52'

if (-not (Test-Path $SketchPath)) {
    throw "Sketch not found: $SketchPath"
}
if (-not (Test-Path $PlatformTarget)) {
    throw "Platform not found: $PlatformTarget"
}

Write-Step "RepoRoot=$RepoRoot"
New-Item -ItemType Directory -Force -Path $Sketchbook | Out-Null
Ensure-Junction -Link $PlatformLink -Target $PlatformTarget

@"
directories:
  user: '$($Sketchbook.Replace('\', '/'))'
"@ | Set-Content -Path $ConfigFile -Encoding UTF8

if (-not $SkipClone) {
    $extRoot = Join-Path $RepoRoot '.external'
    New-Item -ItemType Directory -Force -Path $extRoot | Out-Null
    # ICantMakeThings/Nicenano-NRF52-Supermini-PlatformIO-Support contains paths with `|` invalid on Windows — browse on GitHub instead.
    $refs = @(
        @{ Name = 'nRFMicro-Arduino-Core'; Url = 'https://github.com/pdcook/nRFMicro-Arduino-Core.git' },
        @{ Name = 'platform-nordicnrf52'; Url = 'https://github.com/platformio/platform-nordicnrf52.git' }
    )
    $prevEa = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    foreach ($r in $refs) {
        $dest = Join-Path $extRoot $r.Name
        if (-not (Test-Path (Join-Path $dest '.git'))) {
            Write-Step "clone $($r.Name)"
            & git clone --depth 1 $r.Url $dest
            if ($LASTEXITCODE -ne 0) {
                Write-Host "[usb-ci] WARN: clone failed $($r.Name) (network or git); continuing." -ForegroundColor Yellow
            }
        }
    }
    $ErrorActionPreference = $prevEa
}

$baseFqbn = 'arduinonrf:nrf52:promicro_nrf52840'
$matrix = @(
    @{ Name = 'baseline_26000_app_dfu';       Options = '';                                         ExpectedOrigin = 0x26000 },
    @{ Name = 'serial_nosd_1000_app_dfu';     Options = 'bootloader=promicroserialnosd';            ExpectedOrigin = 0x1000 },
    @{ Name = 'dual_cdc_usbgdbstub_1000';    Options = 'bootloader=promicroserialnosd,usbcdc=enabled,buildprofile=usbgdbstub'; ExpectedOrigin = 0x1000 },
    @{ Name = 'legacy270_app_dfu';          Options = 'bootloader=promicrolegacy';                   ExpectedOrigin = 0x27000 },
    @{ Name = 'cdc_off_app_dfu';            Options = 'usbcdc=disabled';                           ExpectedOrigin = 0x26000 },
    @{ Name = 'no_app_dfu_iface';           Options = 'usbdesc=no_app_dfu';                      ExpectedOrigin = 0x26000 },
    @{ Name = 'combo_legacy270_cdc_off_dfu'; Options = 'bootloader=promicrolegacy,usbcdc=disabled,usbdesc=no_app_dfu'; ExpectedOrigin = 0x27000 },
    @{ Name = 'debug_profile';               Options = 'buildprofile=debug';                       ExpectedOrigin = 0x26000 }
)

$firstFqbn = if ([string]::IsNullOrEmpty($matrix[0].Options)) { $baseFqbn } else { "$baseFqbn`:$($matrix[0].Options)" }
$compilerBin = Get-CompilerBin -ConfigFile $ConfigFile -SketchPath $SketchPath -Fqbn $firstFqbn
$Objdump = Join-Path $compilerBin 'arm-none-eabi-objdump.exe'
if (-not (Test-Path $Objdump)) {
    throw "Missing $Objdump"
}

$buildRoot = Join-Path $RepoRoot 'build\usb_ci'
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
$reportLines = New-Object System.Collections.Generic.List[string]
$reportLines.Add('# USB single-cable CI report')
$reportLines.Add("")
$reportLines.Add('| Case | FQBN suffix | .isr_vector VMA | Expected | OK |')
$reportLines.Add('|------|-------------|-----------------|----------|-----|')

$fail = $false
foreach ($row in $matrix) {
    $fqbn = if ([string]::IsNullOrEmpty($row.Options)) { $baseFqbn } else { "$baseFqbn`:$($row.Options)" }
    $outDir = Join-Path $buildRoot $row.Name
    if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null

    Write-Step "compile $($row.Name)"
    & arduino-cli compile --config-file $ConfigFile --fqbn $fqbn --clean --output-dir $outDir $SketchPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[usb-ci] FAIL compile $($row.Name)" -ForegroundColor Red
        $fail = $true
        $reportLines.Add("| $($row.Name) | $($row.Options) | — | 0x$('{0:X}' -f $row.ExpectedOrigin) | NO |")
        continue
    }

    $elf = Join-Path $outDir 'MinimalUsbSmoke.ino.elf'
    if (-not (Test-Path $elf)) {
        $elf = Get-ChildItem -Path $outDir -Filter '*.elf' -Recurse | Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $elf -or -not (Test-Path $elf)) {
        throw "ELF not found under $outDir"
    }

    $vma = Get-IsrVectorVma -Objdump $Objdump -ElfPath $elf
    $ok = ($vma -eq $row.ExpectedOrigin)
    if (-not $ok) { $fail = $true }
    $flag = if ($ok) { 'YES' } else { 'NO' }
    $reportLines.Add("| $($row.Name) | $($row.Options) | 0x$('{0:X}' -f $vma) | 0x$('{0:X}' -f $row.ExpectedOrigin) | $flag |")
}

$reportLines.Add('')
$reportLines.Add('## [HW_REQUIRED] Phase A / C / USB smoke')
$reportLines.Add('')
$reportLines.Add('On hardware: flash `bootloader=promicroserialnosd` first for clones whose BootReturn preinit diagnostic confirms app start 0x1000; otherwise compare baseline 0x26000 / legacy 0x27000. Confirm new COM / runtime PID 0x00B3 (fallback 0x00B4) / logs from `MinimalUsbSmoke`.')
$reportLines.Add('')
$reportPath = Join-Path $buildRoot 'REPORT.md'
$reportLines | Set-Content -Path $reportPath -Encoding UTF8
Write-Step "wrote $reportPath"

if ($fail) {
    throw 'One or more CI checks failed (see REPORT.md).'
}

Write-Host '[usb-ci] All automated checks passed.' -ForegroundColor Green
