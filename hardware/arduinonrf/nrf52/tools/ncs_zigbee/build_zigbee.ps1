param(
    [ValidateSet('promicro_nrf52840', 'usb_dongle_nrf52840', 'nicenano_v2', 'xiao_nrf52840')]
    [string]$Board = 'promicro_nrf52840',

    [ValidateSet('ncp_usb', 'shell', 'coordinator')]
    [string]$Target = 'ncp_usb',

    [ValidateSet('reference', 'no-softdevice')]
    [string]$ImageLayout = 'reference',

    [AllowEmptyString()]
    [string]$Workspace = '',

    [AllowEmptyString()]
    [string]$ToolchainRoot = '',

    [AllowEmptyString()]
    [string]$PythonRoot = '',

    [ValidateSet('auto', 'always', 'never')]
    [string]$Pristine = 'auto',

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

function Resolve-PythonRoot {
    param([string]$Value)
    if (-not [string]::IsNullOrWhiteSpace($Value)) {
        if (-not (Test-Path -LiteralPath (Join-Path $Value 'python.exe') -PathType Leaf)) {
            throw ('Python root does not contain python.exe: {0}' -f $Value)
        }
        return [System.IO.Path]::GetFullPath($Value)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:CONDA_PREFIX)) {
        if ((Split-Path -Leaf $env:CONDA_PREFIX) -eq 'IronEngineWorld') {
            return [System.IO.Path]::GetFullPath($env:CONDA_PREFIX)
        }
    }
    $ironEngineWorld = 'G:\Anaconda\envs\IronEngineWorld'
    if (Test-Path -LiteralPath (Join-Path $ironEngineWorld 'python.exe') -PathType Leaf) {
        return [System.IO.Path]::GetFullPath($ironEngineWorld)
    }
    return ''
}

function Resolve-ToolchainRoot {
    param([string]$Value)
    $candidates = New-Object 'System.Collections.Generic.List[string]'
    if (-not [string]::IsNullOrWhiteSpace($Value)) {
        $candidates.Add($Value)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:NCS_TOOLCHAIN_ROOT)) {
        $candidates.Add($env:NCS_TOOLCHAIN_ROOT)
    }
    $defaultRoot = 'C:\ncs\toolchains'
    if (Test-Path -LiteralPath $defaultRoot -PathType Container) {
        foreach ($dir in @(Get-ChildItem -LiteralPath $defaultRoot -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)) {
            $candidates.Add($dir.FullName)
        }
    }
    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        $envJson = Join-Path $candidate 'environment.json'
        $gcc = Join-Path $candidate 'opt\zephyr-sdk\arm-zephyr-eabi\bin\arm-zephyr-eabi-gcc.exe'
        if ((Test-Path -LiteralPath $envJson -PathType Leaf) -and (Test-Path -LiteralPath $gcc -PathType Leaf)) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    return ''
}

function Add-PathPrefix {
    param([string[]]$Values)
    $existing = $env:PATH
    $prefix = @($Values | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_) })
    if ($prefix.Count -gt 0) {
        $env:PATH = (($prefix | Select-Object -Unique) -join [System.IO.Path]::PathSeparator) + [System.IO.Path]::PathSeparator + $existing
    }
}

function Set-NcsBuildEnvironment {
    param([string]$ResolvedToolchainRoot, [string]$ResolvedPythonRoot)
    $pathPrefix = New-Object 'System.Collections.Generic.List[string]'
    if (-not [string]::IsNullOrWhiteSpace($ResolvedPythonRoot)) {
        $pathPrefix.Add((Join-Path $ResolvedPythonRoot 'Scripts'))
        $pathPrefix.Add($ResolvedPythonRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($ResolvedToolchainRoot)) {
        foreach ($relative in @(
            '.',
            'mingw64\bin',
            'bin',
            'opt\bin',
            'opt\bin\Scripts',
            'opt\nanopb\generator-bin',
            'opt\zephyr-sdk\arm-zephyr-eabi\bin',
            'opt\zephyr-sdk\riscv64-zephyr-elf\bin'
        )) {
            $pathPrefix.Add((Join-Path $ResolvedToolchainRoot $relative))
        }
        $env:ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr'
        $env:ZEPHYR_SDK_INSTALL_DIR = Join-Path $ResolvedToolchainRoot 'opt\zephyr-sdk'
    }
    Add-PathPrefix -Values $pathPrefix.ToArray()
    $env:PYTHONUTF8 = '1'
    $env:PYTHONIOENCODING = 'utf-8'
}

function Get-ShortBoardName {
    param([string]$BoardName)
    switch ($BoardName) {
        'promicro_nrf52840' { return 'pm40' }
        'usb_dongle_nrf52840' { return 'dongle40' }
        'nicenano_v2' { return 'nnv2' }
        'xiao_nrf52840' { return 'xiao40' }
        default { return $BoardName }
    }
}

function Get-ZigbeeBuildSpec {
    param([string]$BoardName, [string]$TargetName, [string]$WorkspaceRoot, [string]$LayoutName)
    $zephyrBoard = switch ($BoardName) {
        'usb_dongle_nrf52840' { 'nrf52840dongle/nrf52840' }
        default { 'nrf52840dk/nrf52840' }
    }
    switch ($TargetName) {
        'ncp_usb' {
            $suffix = if ($BoardName -eq 'usb_dongle_nrf52840') { 'dongle' } else { 'usb' }
            $cmakeArgs = @('-DFILE_SUFFIX={0}' -f $suffix)
            $referenceOnly = $true
            $sysbuild = $true
            $buildDir = Join-Path $WorkspaceRoot ('build\arduinonrf\{0}\{1}' -f $BoardName, $TargetName)
            if ($LayoutName -eq 'no-softdevice') {
                if ($BoardName -ne 'promicro_nrf52840' -and $BoardName -ne 'nicenano_v2') {
                    throw ('Image layout {0} is currently validated only for no-SoftDevice ProMicro/nice!nano-style bootloaders.' -f $LayoutName)
                }
                $layoutDir = Join-Path $PSScriptRoot 'layouts'
                $extraConf = Join-Path $layoutDir 'no_softdevice_ncp_usb.conf'
                $pmStatic = Join-Path $layoutDir 'pm_static_no_softdevice_ncp_usb.yml'
                $cmakeArgs += @(
                    ('-DEXTRA_CONF_FILE={0}' -f $extraConf),
                    ('-DPM_STATIC_YML_FILE={0}' -f $pmStatic)
                )
                $referenceOnly = $false
                $sysbuild = $false
                $buildDir = Join-Path $WorkspaceRoot ('b\an\{0}\ncp-nosd' -f (Get-ShortBoardName -BoardName $BoardName))
            }
            return [pscustomobject]@{
                AppDir = Join-Path $WorkspaceRoot 'ncs-zigbee\samples\ncp'
                ZephyrBoard = $zephyrBoard
                Sysbuild = $sysbuild
                ForceNoSysbuild = -not $sysbuild
                CMakeArgs = $cmakeArgs
                ReferenceOnly = $referenceOnly
                BuildDir = $buildDir
            }
        }
        'shell' {
            return [pscustomobject]@{
                AppDir = Join-Path $WorkspaceRoot 'ncs-zigbee\samples\shell'
                ZephyrBoard = $zephyrBoard
                Sysbuild = $false
                ForceNoSysbuild = $false
                CMakeArgs = @()
                ReferenceOnly = $true
                BuildDir = Join-Path $WorkspaceRoot ('build\arduinonrf\{0}\{1}' -f $BoardName, $TargetName)
            }
        }
        'coordinator' {
            return [pscustomobject]@{
                AppDir = Join-Path $WorkspaceRoot 'ncs-zigbee\samples\network_coordinator'
                ZephyrBoard = $zephyrBoard
                Sysbuild = $false
                ForceNoSysbuild = $false
                CMakeArgs = @()
                ReferenceOnly = $true
                BuildDir = Join-Path $WorkspaceRoot ('build\arduinonrf\{0}\{1}' -f $BoardName, $TargetName)
            }
        }
    }
}

function Write-Check {
    param([string]$Name, [string]$Value)
    if ([string]::IsNullOrWhiteSpace($Value)) {
        Write-Host ("[missing] {0}" -f $Name) -ForegroundColor Yellow
    } else {
        Write-Host ("[ok]      {0}: {1}" -f $Name, $Value)
    }
}

function Test-PartitionContains {
    param(
        [string[]]$Lines,
        [string]$PartitionName,
        [string]$ExpectedAddress,
        [string]$ExpectedEndAddress
    )
    $header = '{0}:' -f $PartitionName
    $inside = $false
    $addressOk = $false
    $endOk = $false
    foreach ($line in $Lines) {
        if ($line -match '^\S.*:$') {
            $inside = ($line.Trim() -eq $header)
            continue
        }
        if (-not $inside) { continue }
        if ($line.Trim() -eq ('address: {0}' -f $ExpectedAddress)) { $addressOk = $true }
        if ($line.Trim() -eq ('end_address: {0}' -f $ExpectedEndAddress)) { $endOk = $true }
    }
    return ($addressOk -and $endOk)
}

function Assert-NoSoftDevicePartitionLayout {
    param([string]$BuildDir)
    $partitionsPath = Join-Path $BuildDir 'partitions.yml'
    if (-not (Test-Path -LiteralPath $partitionsPath -PathType Leaf)) {
        throw ('Expected partitions.yml was not generated: {0}' -f $partitionsPath)
    }
    $lines = Get-Content -LiteralPath $partitionsPath
    if (-not (Test-PartitionContains -Lines $lines -PartitionName 'app' -ExpectedAddress '0x1000' -ExpectedEndAddress '0xe0000')) {
        throw ('Unsafe no-softdevice partition layout: expected app partition 0x1000..0xE0000 in {0}' -f $partitionsPath)
    }
    if (-not (Test-PartitionContains -Lines $lines -PartitionName 'zboss_nvram' -ExpectedAddress '0xe0000' -ExpectedEndAddress '0xe8000')) {
        throw ('Unsafe no-softdevice partition layout: expected zboss_nvram partition 0xE0000..0xE8000 in {0}' -f $partitionsPath)
    }
    if (-not (Test-PartitionContains -Lines $lines -PartitionName 'zboss_product_config' -ExpectedAddress '0xe8000' -ExpectedEndAddress '0xe9000')) {
        throw ('Unsafe no-softdevice partition layout: expected zboss_product_config partition 0xE8000..0xE9000 in {0}' -f $partitionsPath)
    }
    if (-not (Test-PartitionContains -Lines $lines -PartitionName 'uf2_bootloader' -ExpectedAddress '0xe9000' -ExpectedEndAddress '0x100000')) {
        throw ('Unsafe no-softdevice partition layout: expected uf2_bootloader guard partition 0xE9000..0x100000 in {0}' -f $partitionsPath)
    }
    Write-Host ('[layout]   no-softdevice partitions preserve 0x0000..0x0FFF and 0xE9000..0xFFFFF')
}

$workspacePath = Resolve-Workspace -Value $Workspace
$toolchainPath = Resolve-ToolchainRoot -Value $ToolchainRoot
$pythonPath = Resolve-PythonRoot -Value $PythonRoot
Set-NcsBuildEnvironment -ResolvedToolchainRoot $toolchainPath -ResolvedPythonRoot $pythonPath

$pinsPath = Join-Path $PSScriptRoot 'pins.json'
$pins = Get-Content -Raw -LiteralPath $pinsPath | ConvertFrom-Json
$spec = Get-ZigbeeBuildSpec -BoardName $Board -TargetName $Target -WorkspaceRoot $workspacePath -LayoutName $ImageLayout

Write-Host 'ArduinoNRF nCS Zigbee R23 sidecar'
Write-Host ("  board     : {0}" -f $Board)
Write-Host ("  target    : {0}" -f $Target)
Write-Host ("  layout    : {0}" -f $ImageLayout)
Write-Host ("  zephyr    : {0}" -f $spec.ZephyrBoard)
Write-Host ("  workspace : {0}" -f $workspacePath)
Write-Host ("  toolchain : {0}" -f $(if ([string]::IsNullOrWhiteSpace($toolchainPath)) { '<not found>' } else { $toolchainPath }))
Write-Host ("  python env: {0}" -f $(if ([string]::IsNullOrWhiteSpace($pythonPath)) { '<PATH default>' } else { $pythonPath }))
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
Write-Check -Name 'ZEPHYR_SDK_INSTALL_DIR' -Value $env:ZEPHYR_SDK_INSTALL_DIR

if (-not (Test-Path -LiteralPath $workspacePath)) {
    New-Item -ItemType Directory -Force -Path $workspacePath | Out-Null
}

$westMetaPath = Join-Path $workspacePath '.west'
$zephyrPath = Join-Path $workspacePath 'zephyr'
$addonPath = Join-Path $workspacePath 'ncs-zigbee'

Write-Host ''
Write-Host '[workspace]'
Write-Host ("  root          : {0}" -f $workspacePath)
Write-Host ("  .west         : {0}" -f (Test-Path -LiteralPath $westMetaPath))
Write-Host ("  zephyr dir    : {0}" -f (Test-Path -LiteralPath $zephyrPath))
Write-Host ("  ncs-zigbee dir: {0}" -f (Test-Path -LiteralPath $addonPath))
Write-Host ("  app dir       : {0}" -f $spec.AppDir)

if ($CheckOnly) {
    Write-Host ''
    Write-Host 'Check-only mode: no downloads, no build, no flashing.'
    exit 0
}

if ([string]::IsNullOrWhiteSpace($west)) {
    throw 'west was not found. Install the nRF Connect SDK toolchain, then rerun with -CheckOnly first.'
}

if (-not (Test-Path -LiteralPath $westMetaPath) -or -not (Test-Path -LiteralPath $addonPath)) {
    throw ('No nCS Zigbee workspace found in {0}. Initialize/download it there first; this script intentionally does not download SDKs yet.' -f $workspacePath)
}

if (-not (Test-Path -LiteralPath $spec.AppDir -PathType Container)) {
    throw ('Zigbee application not found: {0}' -f $spec.AppDir)
}

$buildDir = $spec.BuildDir
$westArgs = @('build', '-p', $Pristine, '-b', $spec.ZephyrBoard, '-d', $buildDir, $spec.AppDir)
if ($spec.Sysbuild) {
    $westArgs += @('--sysbuild')
}
if ($spec.ForceNoSysbuild) {
    $westArgs += @('--no-sysbuild')
}
if ($VerboseBuild) {
    $westArgs += @('-v')
}
if ($spec.CMakeArgs.Count -gt 0) {
    $westArgs += @('--')
    $westArgs += $spec.CMakeArgs
}

Write-Host ''
if ($spec.ReferenceOnly) {
    Write-Host 'Reference build: validates the official Nordic Zigbee stack. Do not flash generated full images to Arduino bootloader boards.'
} else {
    Write-Host 'Bootloader-preserving build: generated images must still pass flash_zigbee.ps1 address checks before hardware flashing.'
}
Write-Host ('+ west {0}' -f ($westArgs -join ' '))
Push-Location -LiteralPath $workspacePath
try {
    & $west @westArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}

if ($ImageLayout -eq 'no-softdevice') {
    Assert-NoSoftDevicePartitionLayout -BuildDir $buildDir
}

Write-Host ''
Write-Host '[artifacts]'
foreach ($pattern in @('merged.hex', 'dfu_application.zip', 'zephyr\zephyr.hex', 'zephyr\zephyr.signed.hex', 'zephyr\zephyr.bin')) {
    foreach ($artifact in @(Get-ChildItem -LiteralPath $buildDir -Recurse -Filter (Split-Path -Leaf $pattern) -ErrorAction SilentlyContinue)) {
        Write-Host ("  {0} ({1} bytes)" -f $artifact.FullName, $artifact.Length)
    }
}
exit 0
