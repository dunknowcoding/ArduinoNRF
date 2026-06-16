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

if (-not (Test-Path -LiteralPath $Hex -PathType Leaf)) {
    throw ('Hex file not found: {0}' -f $Hex)
}

$hexPath = [System.IO.Path]::GetFullPath($Hex)
$jlink = Resolve-JLinkExe -Preferred $JLinkExe

Write-Host 'ArduinoNRF nCS Zigbee sidecar flash'
Write-Host ("  board      : {0}" -f $Board)
Write-Host ("  programmer : {0}" -f $Programmer)
Write-Host ("  device     : {0}" -f $Device)
Write-Host ("  hex        : {0}" -f $hexPath)
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
