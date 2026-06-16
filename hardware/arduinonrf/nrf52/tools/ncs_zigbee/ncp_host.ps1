param(
    [AllowEmptyString()]
    [string]$Workspace = '',

    [string]$Version = 'v3.6.0',

    [string]$PackageUrl = 'https://github.com/nrfconnect/ncs-zigbee/releases/download/v1.3.0/ncp_host_v3.6.0.zip',

    [string]$Port = 'COM27',

    [string]$WslDistro = 'Ubuntu',

    [AllowEmptyString()]
    [string]$WslTty = '',

    [switch]$Download,

    [switch]$Extract,

    [switch]$RunSimpleGw,

    [switch]$Force
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

function Get-HostPackageName {
    param([string]$HostVersion)
    $label = if ($HostVersion.StartsWith('v')) { $HostVersion } else { 'v{0}' -f $HostVersion }
    return ('ncp_host_{0}.zip' -f $label)
}

function Resolve-HostRoot {
    param([string]$WorkspaceRoot, [string]$HostVersion)
    return (Join-Path $WorkspaceRoot ('ncp-host\{0}' -f $HostVersion.TrimStart('v')))
}

function Convert-ComToWslTty {
    param([string]$ComPort)
    if ($ComPort -match '^COM(\d+)$') {
        return ('/dev/ttyS{0}' -f $Matches[1])
    }
    return $ComPort
}

function Test-WslDistro {
    param([string]$Distro)
    $wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $wsl) { return $false }
    & $wsl.Source -d $Distro -e sh -lc 'uname -s' *> $null
    return ($LASTEXITCODE -eq 0)
}

function Invoke-Download {
    param([string]$Url, [string]$Destination)
    $gh = Get-Command gh.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($gh -and $Url -match 'github\.com/([^/]+/[^/]+)/releases/download/([^/]+)/([^/]+)$') {
        $repo = $Matches[1]
        $tag = $Matches[2]
        $asset = $Matches[3]
        & $gh.Source release download $tag --repo $repo --pattern $asset --dir (Split-Path -Parent $Destination) --clobber
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        return
    }

    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $curl) {
        throw 'Neither gh.exe nor curl.exe was found; cannot download the official NCP Host package.'
    }
    & $curl.Source -L $Url -o $Destination
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$workspacePath = Resolve-Workspace -Value $Workspace
$hostRoot = Resolve-HostRoot -WorkspaceRoot $workspacePath -HostVersion $Version
$packageName = Get-HostPackageName -HostVersion $Version
$zipPath = Join-Path $hostRoot $packageName
$sourcePath = Join-Path $hostRoot 'src'
$simpleGwPath = Join-Path $sourcePath 'application\simple_gw\simple_gw'
$tty = if ([string]::IsNullOrWhiteSpace($WslTty)) { Convert-ComToWslTty -ComPort $Port } else { $WslTty }

New-Item -ItemType Directory -Force -Path $hostRoot | Out-Null

Write-Host 'ArduinoNRF nCS Zigbee NCP Host'
Write-Host ("  version   : {0}" -f $Version)
Write-Host ("  workspace : {0}" -f $workspacePath)
Write-Host ("  package   : {0}" -f $zipPath)
Write-Host ("  source    : {0}" -f $sourcePath)
Write-Host ("  port      : {0}" -f $Port)
Write-Host ("  wsl tty   : {0}" -f $tty)
Write-Host ("  distro    : {0}" -f $WslDistro)
Write-Host ''

if ($Download -or $Extract -or $RunSimpleGw) {
    if ((-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) -or $Force) {
        Write-Host ('[download] {0}' -f $PackageUrl)
        Invoke-Download -Url $PackageUrl -Destination $zipPath
    } else {
        Write-Host ('[download] reuse {0}' -f $zipPath)
    }
}

if ($Extract -or $RunSimpleGw) {
    if ((Test-Path -LiteralPath $sourcePath -PathType Container) -and $Force) {
        $resolvedHostRoot = (Resolve-Path -LiteralPath $hostRoot).Path
        $resolvedSource = (Resolve-Path -LiteralPath $sourcePath).Path
        if (-not $resolvedSource.StartsWith($resolvedHostRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw ('Refusing to remove outside NCP host cache: {0}' -f $resolvedSource)
        }
        Remove-Item -LiteralPath $resolvedSource -Recurse -Force
    }
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Container)) {
        New-Item -ItemType Directory -Force -Path $sourcePath | Out-Null
        Write-Host ('[extract] {0}' -f $zipPath)
        tar -xf $zipPath -C $sourcePath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } else {
        Write-Host ('[extract] reuse {0}' -f $sourcePath)
    }
}

$wslOk = Test-WslDistro -Distro $WslDistro
Write-Host ('[wsl]     {0}: {1}' -f $WslDistro, $(if ($wslOk) { 'available' } else { 'not available' }))
Write-Host ('[simple]  {0}: {1}' -f $simpleGwPath, $(if (Test-Path -LiteralPath $simpleGwPath -PathType Leaf) { 'present' } else { 'missing' }))

if ($RunSimpleGw) {
    if (-not $wslOk) {
        throw ('WSL distro {0} is not available. Install Ubuntu first, then rerun this command.' -f $WslDistro)
    }
    if (-not (Test-Path -LiteralPath $simpleGwPath -PathType Leaf)) {
        throw ('simple_gw was not found. Rerun with -Download -Extract first: {0}' -f $simpleGwPath)
    }
    $linuxSourcePath = (& wsl.exe -d $WslDistro -e wslpath -a $sourcePath).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($linuxSourcePath)) {
        throw ('Could not translate source path for WSL: {0}' -f $sourcePath)
    }
    $cmd = 'cd "{0}" && chmod +x application/simple_gw/simple_gw && NCP_SLAVE_PTY="{1}" ./application/simple_gw/simple_gw' -f $linuxSourcePath.Replace('"', '\"'), $tty.Replace('"', '\"')
    Write-Host ('+ wsl -d {0} -- sh -lc {1}' -f $WslDistro, $cmd)
    & wsl.exe -d $WslDistro -- sh -lc $cmd
    exit $LASTEXITCODE
}

Write-Host ''
Write-Host 'Status only. Use -Download -Extract to prepare the official host package, and -RunSimpleGw after Ubuntu/WSL is available.'
exit 0
