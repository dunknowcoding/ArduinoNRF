param(
    [AllowEmptyString()]
    [string]$Workspace = '',

    [string]$Version = 'v3.6.0',

    [string]$PackageUrl = 'https://github.com/nrfconnect/ncs-zigbee/releases/download/v1.3.0/ncp_host_v3.6.0.zip',

    [string]$Port = 'COM27',

    [AllowEmptyString()]
    [string]$UsbBusId = '',

    [string]$WslDistro = 'Ubuntu',

    [string]$UbuntuDistribution = 'Ubuntu-22.04',

    [string]$UbuntuLocation = 'G:\WSL\ArduinoNRF-Ubuntu',

    [AllowEmptyString()]
    [string]$WslTty = '',

    [switch]$InstallUbuntu,

    [switch]$AttachUsb,

    [switch]$Download,

    [switch]$Extract,

    [switch]$RunSimpleGw,

    [int]$RunSeconds = 0,

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

function Convert-WslText {
    param([object[]]$Lines)
    $text = ($Lines -join [Environment]::NewLine)
    return ($text -replace "`0", '').Trim()
}

function Test-IsAdministrator {
    $current = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($current)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-UsbipdCommand {
    $cmd = Get-Command usbipd.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { return $cmd.Source }

    $fallback = Join-Path $env:ProgramFiles 'usbipd-win\usbipd.exe'
    if (Test-Path -LiteralPath $fallback -PathType Leaf) { return $fallback }

    return ''
}

function Get-WslStatus {
    $wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $wsl) {
        return [PSCustomObject]@{
            Available = $false
            OptionalComponentMissing = $false
            ExitCode = 127
            Message = 'wsl.exe was not found.'
        }
    }

    $output = & $wsl.Source --status 2>&1
    $exitCode = $LASTEXITCODE
    $message = Convert-WslText -Lines $output
    return [PSCustomObject]@{
        Available = ($exitCode -eq 0)
        OptionalComponentMissing = ($message -match 'OPTIONAL_COMPONENT_REQUIRED')
        ExitCode = $exitCode
        Message = $message
    }
}

function Test-WslDistro {
    param([string]$Distro)
    $wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $wsl) { return $false }
    & $wsl.Source -d $Distro -e sh -lc 'uname -s' *> $null
    return ($LASTEXITCODE -eq 0)
}

function Install-UbuntuWsl {
    param([string]$Distro, [string]$Distribution, [string]$Location)
    $wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $wsl) {
        throw 'wsl.exe was not found. Install Windows Subsystem for Linux first.'
    }

    $status = Get-WslStatus
    if ($status.OptionalComponentMissing) {
        Write-Host '[wsl] Windows optional component is missing; running: wsl --install --no-distribution'
        & $wsl.Source --install --no-distribution
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

        $status = Get-WslStatus
        if ($status.OptionalComponentMissing) {
            throw @"
WSL is installed, but the Windows optional component is still not active.
Open an elevated PowerShell and run:

  wsl --install --no-distribution

Then reboot Windows and rerun this command:

  powershell -NoProfile -ExecutionPolicy Bypass -File hardware\arduinonrf\nrf52\tools\ncs_zigbee\ncp_host.ps1 -InstallUbuntu -WslDistro $Distro -UbuntuLocation "$Location"
"@
        }
    }

    if (Test-WslDistro -Distro $Distro) {
        Write-Host ('[wsl]     {0}: already available' -f $Distro)
        return
    }

    $parent = Split-Path -Parent ([System.IO.Path]::GetFullPath($Location))
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Write-Host ('[wsl] installing {0} as {1} at {2}' -f $Distribution, $Distro, $Location)
    & $wsl.Source --install $Distribution --name $Distro --location $Location --no-launch --web-download
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-UsbipdAttach {
    param([string]$BusId, [string]$Distro)
    if ([string]::IsNullOrWhiteSpace($BusId)) {
        throw 'AttachUsb requires -UsbBusId. Run "usbipd list" and use the BUSID for the Zephyr NCP USB device.'
    }

    $usbipd = Get-UsbipdCommand
    if ([string]::IsNullOrWhiteSpace($usbipd)) {
        throw 'usbipd-win was not found. Install it with: winget install --id dorssel.usbipd-win --source winget'
    }

    Write-Host ('[usbipd] list')
    & $usbipd list
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if (-not (Test-IsAdministrator)) {
        throw @"
usbipd bind requires administrator privileges.
Open an elevated PowerShell and run:

  powershell -NoProfile -ExecutionPolicy Bypass -File "$PSCommandPath" -Port $Port -UsbBusId $BusId -WslDistro $Distro -WslTty /dev/ttyACM0 -AttachUsb
"@
    }

    Write-Host ('[usbipd] bind {0}' -f $BusId)
    & $usbipd bind --busid $BusId --force
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host ('[usbipd] attach {0} -> {1}' -f $BusId, $Distro)
    & $usbipd attach --wsl $Distro --busid $BusId
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
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
Write-Host ("  usb busid : {0}" -f $(if ([string]::IsNullOrWhiteSpace($UsbBusId)) { '(not set)' } else { $UsbBusId }))
Write-Host ("  wsl tty   : {0}" -f $tty)
Write-Host ("  distro    : {0}" -f $WslDistro)
Write-Host ("  run secs  : {0}" -f $(if ($RunSeconds -gt 0) { $RunSeconds } else { 'unlimited' }))
Write-Host ("  ubuntu    : {0}" -f $UbuntuDistribution)
Write-Host ("  location  : {0}" -f $UbuntuLocation)
Write-Host ''

if ($InstallUbuntu) {
    Install-UbuntuWsl -Distro $WslDistro -Distribution $UbuntuDistribution -Location $UbuntuLocation
}

if ($AttachUsb) {
    Invoke-UsbipdAttach -BusId $UsbBusId -Distro $WslDistro
}

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

$wslStatus = Get-WslStatus
$wslOk = if ($wslStatus.Available) { Test-WslDistro -Distro $WslDistro } else { $false }
if ($wslStatus.OptionalComponentMissing) {
    Write-Host '[wsl]     Windows optional component: missing or pending reboot'
    Write-Host '[wsl]     Run elevated: wsl --install --no-distribution, then reboot.'
} elseif (-not $wslStatus.Available) {
    Write-Host ('[wsl]     status: unavailable (exit {0})' -f $wslStatus.ExitCode)
    if (-not [string]::IsNullOrWhiteSpace($wslStatus.Message)) {
        Write-Host ('[wsl]     {0}' -f $wslStatus.Message)
    }
}
Write-Host ('[wsl]     {0}: {1}' -f $WslDistro, $(if ($wslOk) { 'available' } else { 'not available' }))
Write-Host ('[simple]  {0}: {1}' -f $simpleGwPath, $(if (Test-Path -LiteralPath $simpleGwPath -PathType Leaf) { 'present' } else { 'missing' }))

if ($RunSimpleGw) {
    if (-not $wslOk) {
        throw ('WSL distro {0} is not available. Install/enable Ubuntu first, then rerun this command. Use -InstallUbuntu to install it to the configured location when WSL is enabled.' -f $WslDistro)
    }
    if (-not (Test-Path -LiteralPath $simpleGwPath -PathType Leaf)) {
        throw ('simple_gw was not found. Rerun with -Download -Extract first: {0}' -f $simpleGwPath)
    }
    $linuxSourcePath = (& wsl.exe -d $WslDistro -e wslpath -a $sourcePath).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($linuxSourcePath)) {
        throw ('Could not translate source path for WSL: {0}' -f $sourcePath)
    }
    $runner = if ($RunSeconds -gt 0) { 'timeout {0}s ' -f $RunSeconds } else { '' }
    $cmd = 'cd "{0}" && chmod +x application/simple_gw/simple_gw && {1}env NCP_SLAVE_PTY="{2}" ./application/simple_gw/simple_gw' -f $linuxSourcePath.Replace('"', '\"'), $runner, $tty.Replace('"', '\"')
    Write-Host ('+ wsl -d {0} -- sh -lc {1}' -f $WslDistro, $cmd)
    & wsl.exe -d $WslDistro -- sh -lc $cmd
    exit $LASTEXITCODE
}

Write-Host ''
Write-Host 'Status only. Use -Download -Extract to prepare the official host package, -InstallUbuntu to prepare Ubuntu/WSL, -AttachUsb for WSL2 USB pass-through, and -RunSimpleGw after the Linux tty is available.'
exit 0
