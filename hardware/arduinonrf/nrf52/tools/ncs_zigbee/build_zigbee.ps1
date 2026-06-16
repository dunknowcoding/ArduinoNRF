param(
    [ValidateSet('promicro_nrf52840', 'usb_dongle_nrf52840', 'nicenano_v2', 'xiao_nrf52840')]
    [string]$Board = 'promicro_nrf52840',

    [ValidateSet('ncp_usb', 'shell', 'coordinator')]
    [string]$Target = 'ncp_usb',

    [AllowEmptyString()]
    [string]$Workspace = '',

    [switch]$CheckOnly,
    [switch]$VerboseBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-RepoRoot {
    $dir = Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..\..')
    return $dir.Path
}

function Resolve-Workspace {
    param([string]$Value)
    if (-not [string]::IsNullOrWhiteSpace($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }
    return (Join-Path (Get-RepoRoot) '.ncs-zigbee-work')
}

function Find-CommandPath {
    param([string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { return $cmd.Source }
    return ''
}

function Write-Check {
    param([string]$Name, [string]$Value)
    if ([string]::IsNullOrWhiteSpace($Value)) {
        Write-Host ("[missing] {0}" -f $Name) -ForegroundColor Yellow
    } else {
        Write-Host ("[ok]      {0}: {1}" -f $Name, $Value)
    }
}

$workspacePath = Resolve-Workspace -Value $Workspace
$pinsPath = Join-Path $PSScriptRoot 'pins.json'
$pins = Get-Content -Raw -LiteralPath $pinsPath | ConvertFrom-Json

Write-Host 'ArduinoNRF nCS Zigbee R23 sidecar'
Write-Host ("  board     : {0}" -f $Board)
Write-Host ("  target    : {0}" -f $Target)
Write-Host ("  workspace : {0}" -f $workspacePath)
Write-Host ("  add-on    : {0} {1}" -f $pins.zigbee_addon.name, $pins.zigbee_addon.preferred_version)
Write-Host ''

$west = Find-CommandPath 'west'
$cmake = Find-CommandPath 'cmake'
$python = Find-CommandPath 'python'
if ([string]::IsNullOrWhiteSpace($python)) {
    $python = Find-CommandPath 'python3'
}

Write-Check -Name 'west' -Value $west
Write-Check -Name 'cmake' -Value $cmake
Write-Check -Name 'python' -Value $python

if (-not (Test-Path -LiteralPath $workspacePath)) {
    New-Item -ItemType Directory -Force -Path $workspacePath | Out-Null
}

$manifestPath = Join-Path $workspacePath 'west.yml'
$ncsPath = Join-Path $workspacePath 'ncs'
$addonPath = Join-Path $workspacePath 'ncs-zigbee'

Write-Host ''
Write-Host '[workspace]'
Write-Host ("  root          : {0}" -f $workspacePath)
Write-Host ("  west.yml      : {0}" -f (Test-Path -LiteralPath $manifestPath))
Write-Host ("  ncs dir       : {0}" -f (Test-Path -LiteralPath $ncsPath))
Write-Host ("  ncs-zigbee dir: {0}" -f (Test-Path -LiteralPath $addonPath))

if ($CheckOnly) {
    Write-Host ''
    Write-Host 'Check-only mode: no downloads, no build, no flashing.'
    exit 0
}

if ([string]::IsNullOrWhiteSpace($west)) {
    throw 'west was not found. Install the nRF Connect SDK toolchain, then rerun with -CheckOnly first.'
}

if (-not (Test-Path -LiteralPath $manifestPath) -and -not (Test-Path -LiteralPath $addonPath)) {
    throw ('No nCS Zigbee workspace found in {0}. Initialize/download it there first; this script intentionally does not download SDKs yet.' -f $workspacePath)
}

$appDir = Join-Path $workspacePath ('apps\{0}' -f $Target)
$buildDir = Join-Path $workspacePath ('build\{0}\{1}' -f $Board, $Target)
if (-not (Test-Path -LiteralPath $appDir)) {
    throw ('Sidecar app template not materialized yet: {0}. This module currently provides environment checks and policy scaffolding.' -f $appDir)
}

$westArgs = @('build', '-p', 'auto', '-b', $Board, '-d', $buildDir, $appDir)
if ($VerboseBuild) {
    $westArgs += @('-v')
}

Write-Host ''
Write-Host ('+ west {0}' -f ($westArgs -join ' '))
& $west @westArgs
exit $LASTEXITCODE
