param(
    [Parameter(Mandatory = $true)]
    [string]$Hex,

    [ValidateSet('promicro_nrf52840', 'usb_dongle_nrf52840', 'nicenano_v2', 'xiao_nrf52840')]
    [string]$Board = 'promicro_nrf52840',

    [ValidateSet('jlink')]
    [string]$Programmer = 'jlink',

    [AllowEmptyString()]
    [string]$JLinkExe = '',

    [string]$Device = 'nRF52840_xxAA',

    [ValidateSet('no-softdevice', 'softdevice-s140-v6', 'full-image-lab-only')]
    [string]$BootloaderLayout = 'no-softdevice',

    [switch]$AllowBootloaderOverwrite,

    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-JLinkExe {
    param([string]$Preferred)
    $candidates = New-Object 'System.Collections.Generic.List[string]'
    if (-not [string]::IsNullOrWhiteSpace($Preferred)) {
        $candidates.Add($Preferred)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:NIUS_JLINK_PATH)) {
        if (Test-Path -LiteralPath $env:NIUS_JLINK_PATH -PathType Container) {
            $candidates.Add((Join-Path $env:NIUS_JLINK_PATH 'JLink.exe'))
        } else {
            $candidates.Add($env:NIUS_JLINK_PATH)
        }
    }
    foreach ($root in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if ([string]::IsNullOrWhiteSpace($root)) { continue }
        $seggerRoot = Join-Path $root 'SEGGER'
        if (Test-Path -LiteralPath $seggerRoot) {
            foreach ($dir in @(Get-ChildItem -LiteralPath $seggerRoot -Directory -Filter 'JLink*' -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
                $candidates.Add((Join-Path $dir.FullName 'JLink.exe'))
            }
        }
    }
    foreach ($cmd in @(Get-Command JLink.exe -ErrorAction SilentlyContinue)) {
        if ($cmd.Source) { $candidates.Add($cmd.Source) }
    }
    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        if (Test-Path -LiteralPath $candidate) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw 'SEGGER JLink.exe was not found. Install SEGGER J-Link Software or set NIUS_JLINK_PATH.'
}

function Get-BootloaderLayoutInfo {
    param([string]$Layout)
    switch ($Layout) {
        'no-softdevice' {
            return [pscustomobject]@{
                AppStart = [uint32]0x1000
                AppEnd = [uint32]0xE9000
                Description = 'MBR/no-SoftDevice app plus preserved top UF2 bootloader'
            }
        }
        'softdevice-s140-v6' {
            return [pscustomobject]@{
                AppStart = [uint32]0x26000
                AppEnd = [uint32]0xE9000
                Description = 'S140 v6 app plus preserved top UF2 bootloader'
            }
        }
        'full-image-lab-only' {
            return [pscustomobject]@{
                AppStart = [uint32]0x0
                AppEnd = [uint32]0x100000
                Description = 'full 1 MB lab image; may overwrite bootloaders'
            }
        }
    }
}

function Get-IntelHexAddressRange {
    param([string]$Path)
    $upperLinear = [uint32]0
    $upperSegment = [uint32]0
    $min = $null
    $max = $null

    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if (-not $line.StartsWith(':')) {
            throw ('Invalid Intel HEX line in {0}' -f $Path)
        }
        $count = [Convert]::ToInt32($line.Substring(1, 2), 16)
        $offset = [Convert]::ToUInt32($line.Substring(3, 4), 16)
        $recordType = [Convert]::ToInt32($line.Substring(7, 2), 16)

        if ($recordType -eq 0) {
            if ($count -eq 0) { continue }
            $base = $upperLinear + $upperSegment + $offset
            $end = $base + [uint32]$count - 1
            if ($null -eq $min -or $base -lt $min) { $min = $base }
            if ($null -eq $max -or $end -gt $max) { $max = $end }
        } elseif ($recordType -eq 2) {
            $upperSegment = [Convert]::ToUInt32($line.Substring(9, 4), 16) -shl 4
            $upperLinear = [uint32]0
        } elseif ($recordType -eq 4) {
            $upperLinear = [Convert]::ToUInt32($line.Substring(9, 4), 16) -shl 16
            $upperSegment = [uint32]0
        } elseif ($recordType -eq 1) {
            break
        }
    }

    if ($null -eq $min -or $null -eq $max) {
        throw ('No data records found in Intel HEX file: {0}' -f $Path)
    }
    return [pscustomobject]@{ Min = [uint32]$min; Max = [uint32]$max }
}

if (-not (Test-Path -LiteralPath $Hex -PathType Leaf)) {
    throw ('Hex file not found: {0}' -f $Hex)
}

$hexPath = [System.IO.Path]::GetFullPath($Hex)
$range = Get-IntelHexAddressRange -Path $hexPath
$layout = Get-BootloaderLayoutInfo -Layout $BootloaderLayout

if (($range.Min -lt $layout.AppStart) -and -not $AllowBootloaderOverwrite) {
    throw ('Refusing to flash: HEX starts at 0x{0:X}, below protected app_start 0x{1:X}. Use a bootloader-preserving image or pass -AllowBootloaderOverwrite only in a disposable lab setup.' -f $range.Min, $layout.AppStart)
}

if (($range.Max -ge $layout.AppEnd) -and -not $AllowBootloaderOverwrite) {
    throw ('Refusing to flash: HEX ends at 0x{0:X}, at or above protected app_end 0x{1:X}. Use a smaller bootloader-preserving image or pass -AllowBootloaderOverwrite only in a disposable lab setup.' -f $range.Max, $layout.AppEnd)
}

$jlink = Resolve-JLinkExe -Preferred $JLinkExe

Write-Host 'ArduinoNRF nCS Zigbee sidecar flash'
Write-Host ("  board      : {0}" -f $Board)
Write-Host ("  programmer : {0}" -f $Programmer)
Write-Host ("  device     : {0}" -f $Device)
Write-Host ("  layout     : {0} ({1})" -f $BootloaderLayout, $layout.Description)
Write-Host ("  app range  : 0x{0:X}..0x{1:X}" -f $layout.AppStart, ($layout.AppEnd - 1))
Write-Host ("  hex        : {0}" -f $hexPath)
Write-Host ("  hex range  : 0x{0:X}..0x{1:X}" -f $range.Min, $range.Max)
Write-Host ("  jlink      : {0}" -f $jlink)
Write-Host ''
Write-Host 'Policy: application flash only. This script does not run recover, eraseall, or bootloader flashing.'

$scriptPath = Join-Path ([System.IO.Path]::GetTempPath()) ('arduinonrf_zigbee_flash_{0}.jlink' -f ([guid]::NewGuid().ToString('N')))
$commands = @(
    'r',
    'h',
    ('loadfile "{0}"' -f $hexPath),
    'r',
    'g',
    'q'
)

try {
    Set-Content -LiteralPath $scriptPath -Value ($commands -join [Environment]::NewLine) -Encoding ASCII
    $args = @('-device', $Device, '-if', 'SWD', '-speed', '4000', '-autoconnect', '1', '-CommandFile', $scriptPath)
    Write-Host ('+ "{0}" {1}' -f $jlink, ($args -join ' '))
    if ($DryRun) {
        Write-Host 'Dry-run mode: no flashing.'
        exit 0
    }
    & $jlink @args
    exit $LASTEXITCODE
}
finally {
    Remove-Item -LiteralPath $scriptPath -Force -ErrorAction SilentlyContinue
}
