param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('dfu', 'openocd', 'jlink', 'bootloader')]
    [string]$Mode,

    [Parameter(Mandatory = $true)]
    [string]$Tool,

    # All string params below have explicit defaults so that arduino-cli
    # recipes can pass an empty token (e.g. `-Uf2VolumeLabel ""`) without
    # PowerShell rejecting the trailing flag with "Missing an argument".
    [AllowEmptyString()]
    [string]$ScriptRoot = '',
    [AllowEmptyString()]
    [string]$Config = '',
    [AllowEmptyString()]
    [string]$JLinkDevice = '',
    [AllowEmptyString()]
    [string]$ProgrammerProtocol = '',
    [AllowEmptyString()]
    [string]$Hex = '',
    [AllowEmptyString()]
    [string]$UsbVid = '',
    [AllowEmptyString()]
    [string]$UsbPid = '',
    [AllowEmptyString()]
    [string]$RuntimeUsbVid = '',
    [AllowEmptyString()]
    [string]$RuntimeUsbPid = '',
    [AllowEmptyString()]
    [string]$Alt = '',
    [AllowEmptyString()]
    [string]$Bin = '',
    [AllowEmptyString()]
    [string]$Port = '',
    [AllowEmptyString()]
    [string]$UseTouch1200 = 'false',
    [AllowEmptyString()]
    [string]$WaitForUploadPort = 'false',
    [AllowEmptyString()]
    [string]$EnterBootloaderOnly = 'false',
    [AllowEmptyString()]
    [string]$Board = 'nrf52',
    [AllowEmptyString()]
    [string]$BootloaderMode = 'nordic-dfu',
    [AllowEmptyString()]
    [string]$Uf2FamilyId = '',
    [AllowEmptyString()]
    [string]$Uf2AppStart = '0x0',
    [AllowEmptyString()]
    [string]$MaximumSize = '',
    [AllowEmptyString()]
    [string]$RamEnd = '',
    [AllowEmptyString()]
    [string]$FlashEnd = '',
    [AllowEmptyString()]
    [string]$Uf2VolumeLabel = '',
    [AllowEmptyString()]
    [string]$Uf2Model = '',
    [AllowEmptyString()]
    [string]$Uf2BoardId = ''
    ,
    [AllowEmptyString()]
    [string]$SdReq = '',
    [AllowEmptyString()]
    [string]$ArduinoIdeVerboseUpload = 'false',
    # Path to the bundled adafruit-nrfutil(.exe). platform.txt passes
    # {runtime.tools.adafruit-nrfutil.path}/adafruit-nrfutil.exe so serial
    # DFU uses the Boards-Manager-installed binary instead of a Python /
    # Conda install on the user's PATH. Highest priority in resolution.
    [AllowEmptyString()]
    [string]$NrfutilExe = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Verbose mode: set by Arduino IDE 2's "verbose upload" preference (it passes
# -ArduinoIdeVerboseUpload 'true' through platform.txt) or by the env var.
# In quiet (default) mode the console shows only: the banner, the upload
# progress bar, and the final result. All the [nius] internal diagnostics
# (port resolution, same-PID notes, touch pulses, nrfutil path, etc.) are
# gated behind verbose so the normal upload reads like a clean tool.
$script:NiusVerbose = ($ArduinoIdeVerboseUpload -eq 'true' -or $ArduinoIdeVerboseUpload -eq '1' -or $env:NIUS_UPLOAD_VERBOSE -eq '1')
if ($script:NiusVerbose) {
    $VerbosePreference = 'Continue'
}

# Do NOT mirror output to stderr by default. arduino-cli and Arduino IDE 2
# capture BOTH stdout and stderr into the same Output panel, so mirroring
# doubles every line (and the stderr copy renders red). The mirror only
# helps in a plain terminal where stdout is block-buffered; opt in there
# with NIUS_ENABLE_UPLOAD_STDERR_MIRROR=1.
$script:NiusMirrorUploadLinesToStderr = ($env:NIUS_ENABLE_UPLOAD_STDERR_MIRROR -eq '1')

. (Join-Path $PSScriptRoot 'usb_port_helpers.ps1')

function Invoke-NiusConsoleFlush {
    try {
        [Console]::Out.Flush()
        if ($script:NiusMirrorUploadLinesToStderr) {
            [Console]::Error.Flush()
        }
    }
    catch {
    }
}

function Write-NiusHostLine {
    param(
        [Parameter(Position = 0)]
        [AllowEmptyString()]
        [string]$Message = '',
        $ForegroundColor = $null
    )

    if ($null -eq $ForegroundColor) {
        Write-Host $Message
    }
    else {
        Write-Host $Message -ForegroundColor $ForegroundColor
    }

    if ($script:NiusMirrorUploadLinesToStderr) {
        try {
            [Console]::Error.WriteLine($Message)
        }
        catch {
        }
    }

    Invoke-NiusConsoleFlush
}

# Verbose-only detail line. In quiet mode these are suppressed entirely so
# the normal upload output stays clean. Use this for every internal
# [nius] diagnostic that an end user does not need to see.
function Write-NiusDetail {
    param(
        [Parameter(Position = 0)]
        [AllowEmptyString()]
        [string]$Message = '',
        $ForegroundColor = $null
    )

    if (-not $script:NiusVerbose) {
        return
    }
    Write-NiusHostLine -Message $Message -ForegroundColor $ForegroundColor
}

function Write-Banner {
    param([string]$BoardName)

    # NiusRobotLab "slant" figlet, pure ASCII so it renders identically in
    # every host regardless of console codepage. Single-quoted literals keep
    # backslash / backtick literal. Printed once, right after compile. The art
    # is bracketed by the only two *** rules in the whole upload output; the
    # subtitle that follows uses a plain dash and no trailing rule.
    Write-NiusHostLine '*******************************************************************'
    Write-NiusHostLine '    _   ___            ____        __          __  __          __'
    Write-NiusHostLine '   / | / (_)_  _______/ __ \____  / /_  ____  / /_/ /   ____ _/ /_'
    Write-NiusHostLine '  /  |/ / / / / / ___/ /_/ / __ \/ __ \/ __ \/ __/ /   / __ `/ __ \'
    Write-NiusHostLine ' / /|  / / /_/ (__  ) _, _/ /_/ / /_/ / /_/ / /_/ /___/ /_/ / /_/ /'
    Write-NiusHostLine '/_/ |_/_/\__,_/____/_/ |_|\____/_.___/\____/\__/_____/\__,_/_.___/'
    Write-NiusHostLine '*******************************************************************'
    Write-NiusHostLine ('   nRF52 Flash Console - Target: {0}' -f $BoardName)
    Write-NiusHostLine ''

    # Mark the start of the user-visible upload run for the closing summary.
    $script:NiusUploadStartUtc = [datetime]::UtcNow
}

# Closing summary: the final 100% bar plus 2 lines of plain-language status
# (total time, soft reset). The *** rules live only around the banner now, so
# this block is set off by blank lines instead. Pure ASCII.
function Write-NiusUploadComplete {
    param([string]$Note = '')

    Write-Stage -Percent 100 -Label 'Upload complete'

    $elapsed = ''
    if ($script:NiusUploadStartUtc) {
        $sec = [Math]::Round(([datetime]::UtcNow - $script:NiusUploadStartUtc).TotalSeconds, 1)
        $elapsed = '{0}s' -f $sec
    }
    Write-NiusHostLine ''
    if (-not [string]::IsNullOrWhiteSpace($elapsed)) {
        Write-NiusHostLine ('  Total upload time : {0}' -f $elapsed)
    }
    Write-NiusHostLine '  Soft reset        : done - board rebooted into new firmware'
    if (-not [string]::IsNullOrWhiteSpace($Note)) {
        Write-NiusHostLine ('  {0}' -f $Note)
    }
    Write-NiusHostLine ''
}

function Write-NiusBootloaderReady {
    param(
        [AllowEmptyString()]
        [string]$Drive = '',
        [AllowEmptyString()]
        [string]$Note = ''
    )

    Write-Stage -Percent 100 -Label 'UF2 drive ready'

    $elapsed = ''
    if ($script:NiusUploadStartUtc) {
        $sec = [Math]::Round(([datetime]::UtcNow - $script:NiusUploadStartUtc).TotalSeconds, 1)
        $elapsed = '{0}s' -f $sec
    }
    Write-NiusHostLine ''
    if (-not [string]::IsNullOrWhiteSpace($elapsed)) {
        Write-NiusHostLine ('  Total time     : {0}' -f $elapsed)
    }
    if (-not [string]::IsNullOrWhiteSpace($Drive)) {
        Write-NiusHostLine ('  UF2 drive      : {0}' -f $Drive)
    }
    Write-NiusHostLine '  Upload skipped : board left in bootloader'
    if (-not [string]::IsNullOrWhiteSpace($Note)) {
        Write-NiusHostLine ('  {0}' -f $Note)
    }
    Write-NiusHostLine ''
}

# Phase timing - always appended to %TEMP%\nius_upload_timing.log so a single
# real upload reveals where the wall-clock goes (connect vs genpkg vs transfer)
# without needing verbose console output. Cheap; no-op before the banner sets
# the start time.
function Write-NiusTiming {
    param([string]$Label)
    # Off by default; opt in with NIUS_UPLOAD_TIMING=1 to profile where the
    # upload wall-clock goes (connect vs touch vs genpkg vs transfer).
    if ($env:NIUS_UPLOAD_TIMING -ne '1') { return }
    try {
        if (-not $script:NiusUploadStartUtc) { return }
        $ms = [int](([datetime]::UtcNow - $script:NiusUploadStartUtc).TotalMilliseconds)
        Add-Content -LiteralPath (Join-Path $env:TEMP 'nius_upload_timing.log') -Value ('+{0,6} ms  {1}' -f $ms, $Label) -ErrorAction SilentlyContinue
    }
    catch {
    }
}

# Internal section header - verbose only.
function Write-Section {
    param([string]$Label)

    Write-NiusDetail ('[nius] {0}' -f $Label)
}

# Track the last progress line we emitted so milestone-based stages don't
# spam the IDE 2 panel with near-identical lines.
$script:NiusLastStagePercent = -1

function Write-Stage {
    param(
        [int]$Percent,
        [string]$Label,
        [string]$Detail = ''
    )

    $clampedPercent = $Percent
    if ($clampedPercent -lt 0) { $clampedPercent = 0 }
    if ($clampedPercent -gt 100) { $clampedPercent = 100 }

    # NiusRobotLab signature bar: a branded "NIUS" prefix, a comet head that
    # rides a dotted track, and an optional detail field (e.g. byte counts):
    #   NIUS  ==============>.......  64%  Uploading  29.0/45.0 KB
    # No bracket decorations. Pure ASCII so it renders identically regardless
    # of console codepage.
    $width = 22
    $filled = [int][Math]::Round($clampedPercent * $width / 100.0)
    if ($filled -gt $width) { $filled = $width }
    if ($filled -lt 0) { $filled = 0 }
    if ($filled -ge $width) {
        $bar = '=' * $width
    }
    elseif ($filled -le 0) {
        $bar = '.' * $width
    }
    else {
        $bar = (('=' * ($filled - 1)) + '>').PadRight($width, '.')
    }
    $script:NiusLastStagePercent = $clampedPercent
    $line = '  NIUS  {0}  {1,3}%  {2}' -f $bar, $clampedPercent, $Label
    if (-not [string]::IsNullOrWhiteSpace($Detail)) {
        $line = '{0}  {1}' -f $line, $Detail
    }
    Write-NiusHostLine $line
}

# Spinner during long quiet phases (e.g. nrfutil mid-transfer). This is
# verbose-only: in quiet mode the milestone Write-Stage lines are enough
# and the spinner would just spam the IDE panel.
function Write-ProgressPulse {
    param(
        [int]$Percent,
        [string]$Label,
        [int]$Tick
    )

    if (-not $script:NiusVerbose) {
        return
    }
    $frames = @('[-]', '[\]', '[|]', '[/]')
    $frame = $frames[$Tick % $frames.Count]
    $clampedPercent = $Percent
    if ($clampedPercent -lt 0) { $clampedPercent = 0 }
    if ($clampedPercent -gt 100) { $clampedPercent = 100 }
    $filled = [int]($clampedPercent * 20 / 100)
    if ($filled -gt 20) { $filled = 20 }
    $bar = ('=' * $filled).PadRight(20)
    Write-NiusHostLine ('{0} [{1}] {2,3}%  {3}' -f $frame, $bar, $clampedPercent, $Label)
}

function Get-ProgressPulseIntervalTicks {
    param(
        [string]$FailureKind = '',
        [int]$Percent = 0
    )

    if ($FailureKind -eq 'adafruit-dfu' -and $Percent -ge 90) {
        return 8
    }

    if ($FailureKind -eq 'adafruit-genpkg') {
        return 3
    }

    return 1
}

function Show-FailureReport {
    param(
        [string]$Title,
        [string]$Summary,
        [string[]]$Hints,
        [string[]]$Details
    )

    [array]$hintList = @($Hints | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    [array]$detailList = @($Details | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })

    Write-NiusHostLine '----------------------------------------------------------------'
    Write-NiusHostLine ('[nius][fail] {0}' -f $Title)
    Write-NiusHostLine (' reason: {0}' -f $Summary)

    if ($hintList.Count -gt 0) {
        Write-NiusHostLine ' hints:'
        foreach ($hint in $hintList) {
            Write-NiusHostLine ('  - {0}' -f $hint)
        }
    }

    if ($detailList.Count -gt 0) {
        Write-NiusHostLine ' trace:'
        foreach ($detail in $detailList) {
            Write-NiusHostLine ('  > {0}' -f $detail)
        }
    }

    Write-NiusHostLine '----------------------------------------------------------------'
}

function Format-ExitCode {
    param([int]$Code)

    if ($Code -lt 0) {
        return ('{0} (0x{1})' -f $Code, ('{0:X8}' -f ($Code -band 0xFFFFFFFF)))
    }

    return [string]$Code
}

function ConvertTo-WindowsCommandLineArgument {
    param(
        [AllowEmptyString()]
        [string]$Argument = ''
    )

    # ProcessStartInfo.Arguments is one Windows command-line string rather than
    # an argv array. Follow the CommandLineToArgvW/CRT escaping rules so paths
    # ending in a backslash and arguments containing quotes cannot merge with
    # the next uploader option.
    if ($Argument.Length -eq 0) {
        return '""'
    }
    if ($Argument -notmatch '[\s"]') {
        return $Argument
    }

    $escaped = New-Object System.Text.StringBuilder
    [void]$escaped.Append('"')
    $backslashes = 0
    foreach ($character in $Argument.ToCharArray()) {
        if ($character -eq '\') {
            $backslashes += 1
            continue
        }
        if ($character -eq '"') {
            [void]$escaped.Append(('\' * (($backslashes * 2) + 1)))
            [void]$escaped.Append('"')
            $backslashes = 0
            continue
        }
        if ($backslashes -gt 0) {
            [void]$escaped.Append(('\' * $backslashes))
            $backslashes = 0
        }
        [void]$escaped.Append($character)
    }
    if ($backslashes -gt 0) {
        # Backslashes before the closing quote must be doubled.
        [void]$escaped.Append(('\' * ($backslashes * 2)))
    }
    [void]$escaped.Append('"')
    return $escaped.ToString()
}

function ConvertTo-ProcessArguments {
    param([string[]]$Arguments)

    return (($Arguments | ForEach-Object {
        ConvertTo-WindowsCommandLineArgument -Argument $_
    }) -join ' ')
}

function ConvertTo-OpenOcdTclWord {
    param(
        [AllowEmptyString()]
        [string]$Value = ''
    )

    # This value is embedded inside OpenOCD's Tcl `-c` program, so Windows
    # process argument quoting is not sufficient. Reject control characters and
    # suppress Tcl variable/command/backslash substitution inside a quoted word.
    if ($Value -match '[\x00-\x1F\x7F]') {
        throw 'OpenOCD command values must not contain control characters.'
    }
    $escaped = $Value.Replace('\', '\\')
    $escaped = $escaped.Replace('"', '\"')
    $escaped = $escaped.Replace('$', '\$')
    $escaped = $escaped.Replace('[', '\[')
    $escaped = $escaped.Replace(']', '\]')
    return ('"{0}"' -f $escaped)
}

function Resolve-NiusOptionalProbeIdentity {
    param(
        [Parameter(Mandatory = $true)]
        [string]$EnvironmentName
    )

    $value = [Environment]::GetEnvironmentVariable($EnvironmentName)
    if ([string]::IsNullOrWhiteSpace($value)) {
        return ''
    }
    $value = $value.Trim()
    if ($value.Length -gt 128 -or $value -match '[\x00-\x1F\x7F]') {
        throw ('{0} must be a probe serial/nickname of at most 128 characters without control characters.' -f $EnvironmentName)
    }
    return $value
}

function New-NiusJLinkCommandArguments {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Device,
        [Parameter(Mandatory = $true)]
        [string]$CommandFile
    )

    $arguments = @(
        '-Device', $Device,
        '-If', 'SWD',
        '-Speed', '1000',
        '-AutoConnect', '1',
        '-ExitOnError', '1',
        '-NoGui', '1'
    )
    $probeIdentity = Resolve-NiusOptionalProbeIdentity -EnvironmentName 'NIUS_JLINK_SERIAL'
    if (-not [string]::IsNullOrWhiteSpace($probeIdentity)) {
        $arguments += @('-USB', $probeIdentity)
    }
    $arguments += @('-CommanderScript', $CommandFile)
    return $arguments
}

function New-NiusOpenOcdArguments {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptRootPath,
        [Parameter(Mandatory = $true)]
        [string]$ConfigPath,
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    $arguments = @('-s', $ScriptRootPath, '-f', $ConfigPath)
    $probeIdentity = Resolve-NiusOptionalProbeIdentity -EnvironmentName 'NIUS_CMSIS_DAP_SERIAL'
    if (-not [string]::IsNullOrWhiteSpace($probeIdentity)) {
        # Must execute before `init`, otherwise OpenOCD may already have opened
        # the first matching adapter on a multi-probe host.
        $arguments += @('-c', ('adapter serial {0}' -f (ConvertTo-OpenOcdTclWord -Value $probeIdentity)))
    }
    $arguments += @('-c', $Command)
    return $arguments
}

function Assert-ToolExists {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw ('Upload tool not found: {0}. Check the installed Arduino tool layout and the platform upload recipe.' -f $Path)
    }
}

function Test-NiusSeggerJLinkExe {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    if ([System.IO.Path]::GetFileName($Path) -notmatch '^(?i)JLink\.exe$') {
        return $false
    }
    try {
        $item = Get-Item -LiteralPath $Path -ErrorAction Stop
        $versionInfo = $item.VersionInfo
        $text = @($item.FullName, $versionInfo.ProductName, $versionInfo.CompanyName, $versionInfo.FileDescription) -join ' '
        return ($text -match '(?i)SEGGER|J-Link')
    }
    catch {
        return ($Path -match '(?i)\\SEGGER\\')
    }
}

function Resolve-NiusJLinkExe {
    param([string]$Preferred = '')

    $candidates = New-Object 'System.Collections.Generic.List[string]'

    if (-not [string]::IsNullOrWhiteSpace($Preferred) -and $Preferred -ne 'auto') {
        if (Test-Path -LiteralPath $Preferred -PathType Container) {
            $candidates.Add((Join-Path $Preferred 'JLink.exe'))
        }
        else {
            $candidates.Add($Preferred)
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:NIUS_JLINK_PATH)) {
        if (Test-Path -LiteralPath $env:NIUS_JLINK_PATH -PathType Container) {
            $candidates.Add((Join-Path $env:NIUS_JLINK_PATH 'JLink.exe'))
        }
        else {
            $candidates.Add($env:NIUS_JLINK_PATH)
        }
    }

    foreach ($root in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if ([string]::IsNullOrWhiteSpace($root)) { continue }
        $seggerRoot = Join-Path $root 'SEGGER'
        if (Test-Path -LiteralPath $seggerRoot) {
            foreach ($hit in @(Get-ChildItem -LiteralPath $seggerRoot -Directory -Filter 'JLink*' -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
                $candidates.Add((Join-Path $hit.FullName 'JLink.exe'))
            }
        }
    }

    foreach ($cmd in @(Get-Command JLink.exe -ErrorAction SilentlyContinue)) {
        if ($cmd.Source) {
            $candidates.Add($cmd.Source)
        }
    }

    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        if (Test-NiusSeggerJLinkExe -Path $candidate) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    throw 'SEGGER JLink.exe was not found. Install SEGGER J-Link Software, or set NIUS_JLINK_PATH to JLink.exe or its installation directory.'
}

function Assert-InputArtifact {
    param(
        [string]$Path,
        [string]$Label
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        throw ('Required {0} artifact not found: {1}. The sketch may not have compiled correctly, or the Arduino build path did not produce the expected file.' -f $Label, $Path)
    }
}

function Get-Uf2Info {
    param([string]$RootPath)

    $infoPath = Join-Path $RootPath 'INFO_UF2.TXT'
    if (-not (Test-Path -LiteralPath $infoPath)) {
        return $null
    }

    $result = [ordered]@{}
    foreach ($line in Get-Content -LiteralPath $infoPath) {
        if ($line -match '^(?<key>[^:]+):\s*(?<value>.+)$') {
            $result[$matches['key'].Trim()] = $matches['value'].Trim()
        }
    }

    return [pscustomobject]$result
}

function Get-Uf2CandidateRoots {
    $roots = New-Object 'System.Collections.Generic.List[string]'

    try {
        foreach ($drive in @(Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue)) {
            if (-not [string]::IsNullOrWhiteSpace($drive.Root) -and -not $roots.Contains($drive.Root)) {
                $roots.Add($drive.Root)
            }
        }
    } catch {
    }

    for ($code = [int][char]'A'; $code -le [int][char]'Z'; $code++) {
        $root = ('{0}:\' -f [char]$code)
        if ((Test-Path -LiteralPath $root) -and -not $roots.Contains($root)) {
            $roots.Add($root)
        }
    }

    return $roots.ToArray()
}

function Resolve-Uf2CompositeStableIdToken {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $null
    }

    $normalized = $Value.Trim().ToUpperInvariant()
    if ($normalized -match '^[0-9A-F]{16}$') {
        return $normalized
    }

    if ($normalized -match '(?<tail>[0-9A-F]{16})(?:&0)?(?::.*)?$') {
        return $matches['tail']
    }

    return $null
}

function Get-Uf2VolumeCompositeStableId {
    param([string]$RootPath)

    if ([string]::IsNullOrWhiteSpace($RootPath)) {
        return $null
    }

    $driveLetter = $RootPath.Trim().TrimEnd('\')
    if ($driveLetter.Length -lt 1) {
        return $null
    }
    $driveLetter = $driveLetter.Substring(0, 1).ToUpperInvariant()

    $partition = $null
    try {
        $partition = Get-Partition -DriveLetter $driveLetter -ErrorAction Stop | Select-Object -First 1
    }
    catch {
        $partition = $null
    }

    $disk = $null
    if ($partition) {
        try {
            $disk = Get-Disk -Number $partition.DiskNumber -ErrorAction Stop | Select-Object -First 1
        }
        catch {
            $disk = $null
        }
    }

    $diskCandidates = @()
    if ($disk) {
        $diskCandidates += @([string]$disk.SerialNumber, [string]$disk.UniqueId, [string]$disk.Path)
    }
    foreach ($candidate in $diskCandidates) {
        $stableId = Resolve-Uf2CompositeStableIdToken -Value $candidate
        if (-not [string]::IsNullOrWhiteSpace($stableId)) {
            return $stableId
        }
    }

    if ($partition) {
        try {
            $diskDrive = Get-CimInstance Win32_DiskDrive -ErrorAction Stop | Where-Object { $_.Index -eq $partition.DiskNumber } | Select-Object -First 1
            foreach ($candidate in @([string]$diskDrive.PNPDeviceID, [string]$diskDrive.Model, [string]$diskDrive.Caption)) {
                $stableId = Resolve-Uf2CompositeStableIdToken -Value $candidate
                if (-not [string]::IsNullOrWhiteSpace($stableId)) {
                    return $stableId
                }
            }
        }
        catch {
        }
    }

    return $null
}

function Get-Uf2ProbeSummary {
    param(
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = '',
        [string]$PreferredCompositeStableId = ''
    )

    $matches = New-Object 'System.Collections.Generic.List[object]'
    $preferredStableId = ([string]$PreferredCompositeStableId).Trim().ToUpperInvariant()

    foreach ($root in @(Get-Uf2CandidateRoots)) {
        $uf2Info = Get-Uf2Info -RootPath $root
        if ($uf2Info) {
            $driveLetter = $root.Substring(0, 1)
            $volume = Get-Volume -DriveLetter $driveLetter -ErrorAction SilentlyContinue | Select-Object -First 1
            $label = if ($volume) {
                $volume.FileSystemLabel
            } else {
                $logicalDisk = Get-CimInstance Win32_LogicalDisk -Filter ("DeviceID='{0}:'" -f $driveLetter) -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($logicalDisk) { $logicalDisk.VolumeName } else { $null }
            }
            $summary = [pscustomobject]@{
                Drive = $root
                Label = $label
                Model = if ($uf2Info.PSObject.Properties.Name -contains 'Model') { $uf2Info.Model } else { $null }
                BoardId = if ($uf2Info.PSObject.Properties.Name -contains 'Board-ID') { $uf2Info.'Board-ID' } else { $null }
                SoftDevice = if ($uf2Info.PSObject.Properties.Name -contains 'SoftDevice') { $uf2Info.SoftDevice } else { $null }
                CompositeStableId = Get-Uf2VolumeCompositeStableId -RootPath $root
            }

            $labelMatches = [string]::IsNullOrWhiteSpace($ExpectedLabel) -or [string]::IsNullOrWhiteSpace($summary.Label) -or $summary.Label -eq $ExpectedLabel
            $modelMatches = [string]::IsNullOrWhiteSpace($ExpectedModel) -or [string]::IsNullOrWhiteSpace($summary.Model) -or $summary.Model -eq $ExpectedModel
            $boardIdMatches = [string]::IsNullOrWhiteSpace($ExpectedBoardId) -or [string]::IsNullOrWhiteSpace($summary.BoardId) -or $summary.BoardId -eq $ExpectedBoardId
            if ($labelMatches -and $modelMatches -and $boardIdMatches) {
                $matches.Add($summary)
                continue
            }

            if ([string]::IsNullOrWhiteSpace($ExpectedLabel) -and [string]::IsNullOrWhiteSpace($ExpectedModel) -and [string]::IsNullOrWhiteSpace($ExpectedBoardId)) {
                $matches.Add($summary)
            }
        }
    }

    if ($matches.Count -eq 0) {
        return $null
    }

        if (-not [string]::IsNullOrWhiteSpace($preferredStableId)) {
            $preferredMatches = @($matches | Where-Object {
                    ([string]$_.CompositeStableId).Trim().ToUpperInvariant() -eq $preferredStableId
                })
            if ($preferredMatches.Count -gt 0) {
                return $preferredMatches[0]
            }

            return $null
        }

        if ($matches.Count -gt 1) {
            return $null
        }

        return $matches[0]
    }

function New-Uf2ProbeSummaryFromRoot {
    param([string]$RootPath)

    $uf2Info = Get-Uf2Info -RootPath $RootPath
    if (-not $uf2Info) {
        return $null
    }

    $driveLetter = $RootPath.Substring(0, 1)
    $volume = Get-Volume -DriveLetter $driveLetter -ErrorAction SilentlyContinue | Select-Object -First 1
    $label = if ($volume) {
        $volume.FileSystemLabel
    }
    else {
        $logicalDisk = Get-CimInstance Win32_LogicalDisk -Filter ("DeviceID='{0}:'" -f $driveLetter) -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($logicalDisk) { $logicalDisk.VolumeName } else { $null }
    }

    return [pscustomobject]@{
        Drive = $RootPath
        Label = $label
        Model = if ($uf2Info.PSObject.Properties.Name -contains 'Model') { $uf2Info.Model } else { $null }
        BoardId = if ($uf2Info.PSObject.Properties.Name -contains 'Board-ID') { $uf2Info.'Board-ID' } else { $null }
        SoftDevice = if ($uf2Info.PSObject.Properties.Name -contains 'SoftDevice') { $uf2Info.SoftDevice } else { $null }
        CompositeStableId = Get-Uf2VolumeCompositeStableId -RootPath $RootPath
    }
}

function Get-Uf2ProbeSummaryForStableId {
    param([string]$StableId)

    if ([string]::IsNullOrWhiteSpace($StableId)) {
        return $null
    }

    $want = $StableId.Trim().ToUpperInvariant()
    foreach ($root in @(Get-Uf2CandidateRoots)) {
        $summary = New-Uf2ProbeSummaryFromRoot -RootPath $root
        if (-not $summary) {
            continue
        }
        if (([string]$summary.CompositeStableId).Trim().ToUpperInvariant() -eq $want) {
            return $summary
        }
    }

    return $null
}

function Resolve-NiusLayoutGuardUf2Summary {
    param(
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = '',
        [string]$PreferredCompositeStableId = '',
        [int]$WaitMs = 0,
        [string]$PortName = '',
        [switch]$AllowRetouch
    )

    function Get-LayoutGuardCandidate {
        $summary = $null
        try {
            $summary = Get-Uf2ProbeSummary -ExpectedLabel $ExpectedLabel -ExpectedModel $ExpectedModel -ExpectedBoardId $ExpectedBoardId -PreferredCompositeStableId $PreferredCompositeStableId
        }
        catch {
            $summary = $null
        }
        if (-not $summary -and -not [string]::IsNullOrWhiteSpace($PreferredCompositeStableId)) {
            $summary = Get-Uf2ProbeSummaryForStableId -StableId $PreferredCompositeStableId
        }
        return $summary
    }

    $deadline = if ($WaitMs -gt 0) { (Get-Date).AddMilliseconds($WaitMs) } else { Get-Date }
    do {
        $summary = Get-LayoutGuardCandidate
        if ($summary) {
            return $summary
        }
        if ((Get-Date) -ge $deadline) {
            break
        }
        Start-Sleep -Milliseconds 400
    } while ($true)

    if (-not $AllowRetouch -or [string]::IsNullOrWhiteSpace($PortName)) {
        return $null
    }

    $portState = Get-SerialPortUsableState -PortName $PortName
    if (-not $portState.Openable) {
        return $null
    }

    Write-NiusDetail ('[nius] layout guard: waiting for scoped UF2 on {0} after extra 1200 bps touch...' -f $PortName) -ForegroundColor DarkGray
    $touch = Touch-SerialPort1200 -PortName $PortName
    if (-not $touch.Triggered) {
        return $null
    }

    $retouchDeadline = (Get-Date).AddMilliseconds([Math]::Max($WaitMs, 8000))
    while ((Get-Date) -lt $retouchDeadline) {
        $summary = Get-LayoutGuardCandidate
        if ($summary) {
            return $summary
        }
        # The fast registry snapshot is identity-scoped and normally completes
        # in well under one poll interval; 150 ms keeps enumeration responsive
        # without busy-spinning while the application USB composite returns.
        Start-Sleep -Milliseconds 150
    }

    return $null
}

    function Resolve-PythonLaunch {
        function New-PythonLaunch {
            param(
                [string]$Exe,
                [string[]]$PrefixArgs = @()
            )
            return [pscustomobject]@{
                Exe = $Exe
                PrefixArgs = @($PrefixArgs)
            }
        }

        function Test-PythonLaunch {
            param(
                [string]$Exe,
                [string[]]$PrefixArgs = @()
            )

            if ([string]::IsNullOrWhiteSpace($Exe)) {
                return $false
            }

            try {
                $null = & $Exe @PrefixArgs -c 'import sys; raise SystemExit(0 if sys.version_info.major == 3 else 1)' 2>$null
                return ($LASTEXITCODE -eq 0)
            }
            catch {
                return $false
            }
        }

        function Resolve-ConfiguredPythonPath {
            param([string]$Path)

            if ([string]::IsNullOrWhiteSpace($Path)) {
                return $null
            }

            $candidate = $Path.Trim().Trim('"')
            if (Test-Path -LiteralPath $candidate -PathType Container) {
                foreach ($rel in @('Scripts\python.exe', 'bin\python.exe', 'bin\python')) {
                    $pythonPath = Join-Path $candidate $rel
                    if (Test-Path -LiteralPath $pythonPath) {
                        return (Resolve-Path -LiteralPath $pythonPath).Path
                    }
                }
            }

            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }

            return $null
        }

        if (-not [string]::IsNullOrWhiteSpace($env:NIUS_UF2_PYTHON_EXE)) {
            $explicit = Resolve-ConfiguredPythonPath -Path $env:NIUS_UF2_PYTHON_EXE
            if ($explicit -and (Test-PythonLaunch -Exe $explicit)) {
                return New-PythonLaunch -Exe $explicit
            }
        }

        if (-not [string]::IsNullOrWhiteSpace($env:NIUS_UF2_VENV)) {
            $venvPython = Resolve-ConfiguredPythonPath -Path $env:NIUS_UF2_VENV
            if ($venvPython -and (Test-PythonLaunch -Exe $venvPython)) {
                return New-PythonLaunch -Exe $venvPython
            }
        }

        $conda = Get-Command conda -ErrorAction SilentlyContinue
        if ($conda -and -not [string]::IsNullOrWhiteSpace($env:NIUS_UF2_CONDA_ENV)) {
            $condaEnvLaunch = New-PythonLaunch -Exe $conda.Source -PrefixArgs @('run', '-n', $env:NIUS_UF2_CONDA_ENV.Trim(), 'python')
            if (Test-PythonLaunch -Exe $condaEnvLaunch.Exe -PrefixArgs $condaEnvLaunch.PrefixArgs) {
                return $condaEnvLaunch
            }
        }

        foreach ($cmdName in @('python', 'python3')) {
            foreach ($cmd in @(Get-Command $cmdName -All -ErrorAction SilentlyContinue)) {
                $cmdPath = ''
                foreach ($propertyName in @('Source', 'Path', 'Definition')) {
                    $property = $cmd.PSObject.Properties[$propertyName]
                    if ($property -and -not [string]::IsNullOrWhiteSpace([string]$property.Value)) {
                        $cmdPath = [string]$property.Value
                        break
                    }
                }

                # Windows Store/App Execution Alias launchers commonly live in
                # WindowsApps. Judge every candidate by whether it runs Python 3,
                # not by its installation path.
                if (Test-PythonLaunch -Exe $cmdPath) {
                    return New-PythonLaunch -Exe $cmdPath
                }
            }
        }

        $localPythonRoot = Join-Path $env:LOCALAPPDATA 'Programs\Python'
        if (Test-Path -LiteralPath $localPythonRoot -PathType Container) {
            $localInstalls = @(
                Get-ChildItem -LiteralPath $localPythonRoot -Directory -Filter 'Python*' -ErrorAction SilentlyContinue |
                    Sort-Object Name -Descending
            )
            foreach ($install in $localInstalls) {
                $cand = Join-Path $install.FullName 'python.exe'
                if ((Test-Path -LiteralPath $cand -PathType Leaf) -and (Test-PythonLaunch -Exe $cand)) {
                    return New-PythonLaunch -Exe $cand
                }
            }
        }

        $windowsAppsPython = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\python.exe'
        if ((Test-Path -LiteralPath $windowsAppsPython -PathType Leaf) -and (Test-PythonLaunch -Exe $windowsAppsPython)) {
            return New-PythonLaunch -Exe $windowsAppsPython
        }

        # The Windows Python launcher is independent of PATH entries for each
        # installed interpreter, so it remains a useful fallback.
        foreach ($py in @(Get-Command py -All -ErrorAction SilentlyContinue)) {
            $pyPath = if (-not [string]::IsNullOrWhiteSpace($py.Source)) { $py.Source } else { $py.Path }
            if (Test-PythonLaunch -Exe $pyPath -PrefixArgs @('-3')) {
                return New-PythonLaunch -Exe $pyPath -PrefixArgs @('-3')
            }
        }

        if ($conda) {
            try {
                $condaBase = (& $conda.Source info --base 2>$null | Select-Object -First 1).Trim()
                $basePython = Resolve-ConfiguredPythonPath -Path $condaBase
                if ($basePython -and (Test-PythonLaunch -Exe $basePython)) {
                    return New-PythonLaunch -Exe $basePython
                }
            }
            catch {
            }
        }
        throw 'Python 3 could not be started by the Arduino IDE process. Restart Arduino IDE after installing Python, or set NIUS_UF2_PYTHON_EXE to the full python.exe path. NIUS_UF2_VENV and NIUS_UF2_CONDA_ENV are also supported.'
    }

    function Wait-ForUsbBootloader {
        param(
            [string]$Exe,
            [string]$UsbVid,
            [string]$UsbPid,
            [string]$ExpectedLabel,
            [string]$ExpectedModel,
            [string]$ExpectedBoardId,
            [string]$PreferredCompositeStableId = '',
            [bool]$ExpectUf2 = $false,
            # Poll every 150 ms (was 500) so the bootloader/UF2 is detected sooner
            # after the touch; Attempts scaled to keep the ~10 s timeout unchanged.
            [int]$Attempts = 67,
            [int]$DelayMs = 150
        )

        for ($attempt = 0; $attempt -lt $Attempts; $attempt++) {
            if ($ExpectUf2) {
                $uf2 = Get-Uf2ProbeSummary -ExpectedLabel $ExpectedLabel -ExpectedModel $ExpectedModel -ExpectedBoardId $ExpectedBoardId -PreferredCompositeStableId $PreferredCompositeStableId
                if ($uf2) {
                    return [pscustomobject]@{
                        Kind = 'uf2'
                        Summary = $uf2
                    }
                }
            }

            $probeOutput = ''
            $probeExitCode = 0
            $savedErrorActionPreference = $ErrorActionPreference
            try {
                $ErrorActionPreference = 'Continue'
                $probeOutput = & $Exe -l 2>&1 | Out-String
                $probeExitCode = $LASTEXITCODE
            }
            finally {
                $ErrorActionPreference = $savedErrorActionPreference
            }

            $dfuListed = $probeExitCode -eq 0 -and $probeOutput -match [Regex]::Escape(('{0}:{1}' -f $UsbVid, $UsbPid).ToLower())
            $scopedDfuPresent = $true
            if ($dfuListed -and -not [string]::IsNullOrWhiteSpace($PreferredCompositeStableId)) {
                $vidLetters = $UsbVid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
                $pidLetters = $UsbPid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
                $scopedDfuPresent = @(Get-PnpVidPidMatches -VidLetters $vidLetters -PidLetters $pidLetters -PreferredCompositeStableId $PreferredCompositeStableId).Count -gt 0
            }
            if ($dfuListed -and $scopedDfuPresent) {
                return [pscustomobject]@{
                    Kind = 'dfu'
                    Summary = $null
                }
            }

            Start-Sleep -Milliseconds $DelayMs
        }

        return $null
    }

# Known nRF52 bootloader USB identities, ordered by likelihood for the
# AliExpress / nice!nano-class clone fleet. Adafruit-fork UF2 PIDs come first
# because they are by far the most common on these boards; Nordic Open DFU
# Nordic's official USB-serial bootloader identity follows as a fallback. Each
# entry carries the link-layout values
# (uf2_app_start, maximum_size) that most commonly pair with that bootloader.
# Some clone families also expose explicit legacy menu entries where the same
# bootloader PID can be paired with an older app start such as 0x27000.
# For 0x239A:* (Adafruit-fork) bootloaders we prefer the `adafruit-dfu`
# transport (CDC + adafruit-nrfutil) over `uf2` mass-storage drag-drop because
# the host-side MSC LUN is occasionally not promoted to a DiskDrive on
# Windows; CDC-side programming is unaffected by that. UF2 stays available as
# an explicit menu choice for users with healthy MSC mounting.
$script:NrfBootloaderCandidates = @(
    @{ Vid = '239a'; Pid = '00b3'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x26000'; MaxSize = 815104;  Note = 'Adafruit nice!nano v2 / clone fork (default clone layout; legacy 0x27000 remains explicit menu-only)'; VolumeLabel = 'NICENANO'; Model = 'nice!nano'; BoardId = 'nRF52840-nicenano' },
    @{ Vid = '239a'; Pid = '0029'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x26000'; MaxSize = 815104;  Note = 'Adafruit Feather nRF52840 Express UF2 bootloader'; VolumeLabel = 'FTHR840BOOT'; Model = 'Adafruit Feather nRF52840 Express'; BoardId = 'nRF52840-Feather-revE' },
    @{ Vid = '239a'; Pid = '002a'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x26000'; MaxSize = 815104;  Note = 'Adafruit Feather nRF52840 Express CDC-only bootloader identity'; VolumeLabel = 'FTHR840BOOT'; Model = 'Adafruit Feather nRF52840 Express'; BoardId = 'nRF52840-Feather-revE' },
    @{ Vid = '239a'; Pid = '4029'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x26000'; MaxSize = 876544;  Note = 'Adafruit user-mode CDC (nRF52840)' },
    # Seeed XIAO nRF52840 (and XIAO Sense): Adafruit-fork bootloader under Seeed's VID.
    @{ Vid = '2886'; Pid = '0044'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x27000'; MaxSize = 811008;  Note = 'Seeed XIAO nRF52840 (Adafruit-fork, Seeed)'; VolumeLabel = 'XIAO-BOOT'; Model = 'Seeed XIAO nRF52840'; BoardId = 'nRF52840-SeeedXiao-v1' },
    @{ Vid = '2886'; Pid = '8044'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x27000'; MaxSize = 811008;  Note = 'Seeed XIAO nRF52840 (runtime/app PID)'; VolumeLabel = 'XIAO-BOOT'; Model = 'Seeed XIAO nRF52840'; BoardId = 'nRF52840-SeeedXiao-v1' },
    @{ Vid = '2886'; Pid = '0045'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x27000'; MaxSize = 811008;  Note = 'Seeed XIAO nRF52840 Sense'; VolumeLabel = 'XIAO-SENSE'; Model = 'Seeed XIAO nRF52840 Sense'; BoardId = 'nRF52840-SeeedXiaoSense-v1' },
    @{ Vid = '2886'; Pid = '8045'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x27000'; MaxSize = 811008;  Note = 'Seeed XIAO nRF52840 Sense (runtime/app PID)'; VolumeLabel = 'XIAO-SENSE'; Model = 'Seeed XIAO nRF52840 Sense'; BoardId = 'nRF52840-SeeedXiaoSense-v1' },
    # 0x2886:0xF00F is shared by multiple MakerDiary boards. Auto mode may use
    # the USB identity and INFO_UF2 layout, but must not invent one board's
    # volume/model as proof for another. Explicit M.2 recipes carry exact metadata.
    @{ Vid = '2886'; Pid = 'f00f'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x26000'; MaxSize = 815104;  Note = 'MakerDiary shared nRF52840 bootloader identity'; VolumeLabel = ''; Model = ''; BoardId = '' },
    # Makerdiary Pitaya Go: Adafruit-fork bootloader, same VID:PID in DFU and app.
    @{ Vid = '2886'; Pid = 'f00e'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x26000'; MaxSize = 815104;  Note = 'Makerdiary Pitaya Go (Adafruit-fork)'; VolumeLabel = 'PITAYAGO'; Model = 'Makerdiary Pitaya Go'; BoardId = 'PITAYAGO' },
    # nRFMicro (joric open-hardware): Adafruit-fork bootloader under the pid.codes 1209 VID.
    @{ Vid = '1209'; Pid = '5284'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x27000'; MaxSize = 794624;  Note = 'nRFMicro (current Adafruit-fork S140 7.3.0; legacy S140 6.1.1 remains explicit menu-only)'; VolumeLabel = 'NRFMICRO'; Model = 'nRFMicro'; BoardId = 'nRF52840-nRFMicro-v0' },
    @{ Vid = '1915'; Pid = '521f'; Kind = 'nordic-dfu';   Family = '';           AppStart = '0x1000';  MaxSize = 897024;  Note = 'Nordic PCA10059 USB serial DFU (MBR plus onboard bootloader)' }
)

function Resolve-Uf2FlashLayout {
    param(
        [object]$Uf2Summary,
        [string]$DefaultAppStart,
        [int]$DefaultMaxSize,
        [string]$DefaultNote
    )

    $appStart = $DefaultAppStart
    $maxSize = $DefaultMaxSize
    $note = $DefaultNote

    if ($Uf2Summary -and ($Uf2Summary.PSObject.Properties.Name -contains 'SoftDevice')) {
        $softDevice = ([string]$Uf2Summary.SoftDevice).Trim()
        if (-not [string]::IsNullOrWhiteSpace($softDevice)) {
            $softDeviceLower = $softDevice.ToLowerInvariant()
            if ($softDeviceLower -match 'not\s+found|none|no\s+softdevice') {
                $appStart = '0x1000'
                if ($maxSize -le 0 -or $maxSize -gt 950272) {
                    $maxSize = 950272
                }
            }
            elseif ($softDeviceLower -match 's140.*(\bv?7\b|7\.)') {
                $appStart = '0x27000'
                if ($maxSize -le 0 -or $maxSize -gt 811008) {
                    $maxSize = 811008
                }
            }
            elseif ($softDeviceLower -match 's140.*(\bv?6\b|6\.)') {
                $appStart = '0x26000'
                if ($maxSize -le 0 -or $maxSize -gt 815104) {
                    $maxSize = 815104
                }
            }

            $note = ('{0}; INFO_UF2 SoftDevice="{1}" -> app start {2}' -f $DefaultNote, $softDevice, $appStart)
        }
    }

    return [pscustomobject]@{
        AppStart = $appStart
        MaxSize = $maxSize
        Note = $note
    }
}

function Normalize-NiusHexAddress {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ''
    }

    $text = $Value.Trim().ToLowerInvariant()
    try {
        if ($text.StartsWith('0x')) {
            return ('0x{0:x}' -f [Convert]::ToInt64($text.Substring(2), 16))
        }
        return ('0x{0:x}' -f [Convert]::ToInt64($text, 10))
    }
    catch {
        return $text
    }
}

function Assert-Uf2BuildLayoutMatchesBootloader {
    param(
        [object]$Uf2Summary,
        [string]$ExpectedAppStart,
        [string]$Context
    )

    if (-not $Uf2Summary) {
        return
    }

    $layout = Resolve-Uf2FlashLayout -Uf2Summary $Uf2Summary -DefaultAppStart $ExpectedAppStart -DefaultMaxSize 0 -DefaultNote $Context
    $expected = Normalize-NiusHexAddress -Value $ExpectedAppStart
    $actual = Normalize-NiusHexAddress -Value $layout.AppStart
    if ([string]::IsNullOrWhiteSpace($expected) -or [string]::IsNullOrWhiteSpace($actual) -or $expected -eq $actual) {
        return
    }

    $softDevice = if ($Uf2Summary.PSObject.Properties.Name -contains 'SoftDevice') { ([string]$Uf2Summary.SoftDevice).Trim() } else { '' }
    Throw-NiusUploadFailure (New-UploadFailure -Kind 'layout' -ExitCode 1 -Output (@(
                ('The selected Arduino IDE bootloader option compiled this sketch for app start {0}, but the mounted UF2 bootloader reports app start {1}.' -f $expected, $actual),
                ('UF2 drive={0}; label="{1}"; model="{2}"; board-id="{3}"; SoftDevice="{4}".' -f $Uf2Summary.Drive, $Uf2Summary.Label, $Uf2Summary.Model, $Uf2Summary.BoardId, $softDevice),
                'Select the matching Bootloader / DFU layout before compiling. For no-SoftDevice / nice!nano-style clone firmware, use an option that says "no SoftDevice / MBR only (0x1000)".',
                'ZH: Compiled app start does not match the UF2 bootloader flash layout. Select the matching 0x1000/0x26000/0x27000 Bootloader / DFU option, then compile and upload again.'
            ) -join [Environment]::NewLine) -Exe $toolPath)
}

function Invoke-NiusPreUploadLayoutGuard {
    param(
        [string]$ExpectedAppStart,
        [string]$Context,
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = '',
        [string]$PreferredCompositeStableId = '',
        [switch]$RequireUf2Evidence,
        [int]$Uf2ProbeWaitMs = 0,
        [string]$PortName = ''
    )

    $waitMs = $Uf2ProbeWaitMs
    if ($RequireUf2Evidence -and $waitMs -lt 8000) {
        $waitMs = 8000
    }

    $uf2Summary = Resolve-NiusLayoutGuardUf2Summary `
        -ExpectedLabel $ExpectedLabel `
        -ExpectedModel $ExpectedModel `
        -ExpectedBoardId $ExpectedBoardId `
        -PreferredCompositeStableId $PreferredCompositeStableId `
        -WaitMs $waitMs `
        -PortName $PortName `
        -AllowRetouch:$RequireUf2Evidence

    if ($uf2Summary) {
        Assert-Uf2BuildLayoutMatchesBootloader -Uf2Summary $uf2Summary -ExpectedAppStart $ExpectedAppStart -Context $Context
        return
    }

    if (-not $RequireUf2Evidence) {
        return
    }

    $expected = Normalize-NiusHexAddress -Value $ExpectedAppStart
    Throw-NiusUploadFailure (New-UploadFailure -Kind 'layout' -ExitCode 1 -Output (@(
                'Serial DFU upload requires a UF2 volume on the selected board to verify bootloader layout before transfer.',
                ('This sketch was compiled for app start {0}.' -f $(if ([string]::IsNullOrWhiteSpace($expected)) { $ExpectedAppStart } else { $expected })),
                'Double-tap RESET on the selected board to expose the UF2 drive, then upload again; no firmware was written.',
                'If the UF2 drive never appears during serial DFU on Windows, use an explicit UF2 mass-storage Bootloader / DFU menu entry instead.',
                'ZH: Serial DFU cannot verify layout without INFO_UF2.TXT on the selected board. Enter UF2 first, fix the Bootloader / DFU menu if needed, then upload again.'
            ) -join [Environment]::NewLine) -Exe $toolPath)
}

function Invoke-NiusRecoverBoardToBootloader {
    param(
        [string]$PortName,
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = '',
        [string]$PreferredCompositeStableId = '',
        [string]$BootloaderVid = '',
        [string]$BootloaderPid = '',
        [int]$Uf2WaitMs = 18000
    )

    $recovered = $false
    $detail = 'no recovery path attempted'

    if (-not [string]::IsNullOrWhiteSpace($PortName)) {
        $st = Get-SerialPortUsableState -PortName $PortName
        $stableIdMatches = [string]::IsNullOrWhiteSpace($PreferredCompositeStableId) -or
            ((Get-SerialPortUsbParentCompositeStableId -PortName $PortName -Fresh) -eq $PreferredCompositeStableId.Trim().ToUpperInvariant())
        if ($st.Openable -and $stableIdMatches) {
            Write-NiusDetail ('[nius] misflash recovery: 1200 bps touch on {0} to re-enter bootloader...' -f $PortName) -ForegroundColor Yellow
            $touch = Touch-SerialPort1200 -PortName $PortName
            if ($touch.Triggered) {
                $detail = 'one 1200 bps touch sent on service COM'
            }
        }
    }

    $uf2Deadline = (Get-Date).AddMilliseconds($Uf2WaitMs)
    while ((Get-Date) -lt $uf2Deadline) {
        try {
            $uf2 = Get-Uf2ProbeSummary -ExpectedLabel $ExpectedLabel -ExpectedModel $ExpectedModel -ExpectedBoardId $ExpectedBoardId -PreferredCompositeStableId $PreferredCompositeStableId
            if ($uf2) {
                $recovered = $true
                $detail = ('UF2 drive mounted at {0}' -f $uf2.Drive)
                Write-NiusBootloaderReady -Drive $uf2.Drive -Note 'Misflash recovery: board is back in UF2/DFU - fix Bootloader / DFU menu, recompile, upload again.'
                break
            }
        }
        catch {
        }

        if (-not [string]::IsNullOrWhiteSpace($PortName) -and
            -not [string]::IsNullOrWhiteSpace($BootloaderVid) -and
            -not [string]::IsNullOrWhiteSpace($BootloaderPid)) {
            $st = Get-SerialPortUsableState -PortName $PortName
            $stableIdMatches = [string]::IsNullOrWhiteSpace($PreferredCompositeStableId) -or
                ((Get-SerialPortUsbParentCompositeStableId -PortName $PortName -Fresh) -eq $PreferredCompositeStableId.Trim().ToUpperInvariant())
            if ($st.Openable -and $stableIdMatches -and
                (Test-SerialPortMatchesUsbIdentity -PortName $PortName -Vid $BootloaderVid -ProductId $BootloaderPid)) {
                $recovered = $true
                $detail = ('bootloader COM {0} is openable with the expected USB identity' -f $PortName)
                break
            }
        }

        Start-Sleep -Milliseconds 400
    }

    return [pscustomobject]@{
        Recovered = $recovered
        Detail = $detail
    }
}

function Get-NiusRuntimeComNamesForIdentity {
    param(
        [string]$RuntimeVid,
        [string]$RuntimePid,
        [string]$PreferredCompositeStableId = '',
        [switch]$Fresh
    )

    if ([string]::IsNullOrWhiteSpace($RuntimeVid) -or
        [string]::IsNullOrWhiteSpace($RuntimePid)) {
        return @()
    }

    $fastSnapshot = Get-NiusFastUsbSerialRegistrySnapshot `
        -Vid $RuntimeVid -ProductId $RuntimePid `
        -PreferredCompositeStableId $PreferredCompositeStableId
    if ($fastSnapshot.Available) {
        return @($fastSnapshot.Matches |
                ForEach-Object { ([string]$_.DeviceID).Trim().ToUpperInvariant() } |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
                Sort-Object -Unique)
    }

    $vid = $RuntimeVid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $normalizedPid =
        $RuntimePid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $prefix =
        ('USB\VID_{0}&PID_{1}' -f $vid, $normalizedPid).ToUpperInvariant()
    $preferredStable = $PreferredCompositeStableId.Trim().ToUpperInvariant()
    return @(
        Get-SerialPortInventory -Fresh:$Fresh |
            Where-Object {
                $pnpId = ([string]$_.PNPDeviceID).Trim().ToUpperInvariant()
                $identityMatches = $pnpId.StartsWith($prefix)
                if (-not $identityMatches) { return $false }
                if ([string]::IsNullOrWhiteSpace($preferredStable)) { return $true }
                return (Get-UsbInterfaceParentCompositeStableId -PnpInstanceId $pnpId) -eq $preferredStable
            } |
            ForEach-Object { ([string]$_.DeviceID).Trim().ToUpperInvariant() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
}

function Invoke-NiusMisflashGuardAfterSamePidUpload {
    param(
        [string]$PortName,
        [string]$FallbackRuntimePortName = '',
        [string]$RuntimeVid = '',
        [string]$RuntimePid = '',
        [string]$BootloaderVid = '',
        [string]$BootloaderPid = '',
        [string[]]$RuntimePortsBefore = @(),
        [string]$ExpectedAppStart,
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = '',
        [string]$PreferredCompositeStableId = '',
        [int]$TimeoutMs = 12000
    )

    Write-NiusTiming 'post-upload runtime verification start'
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    $before = @{}
    foreach ($name in @($RuntimePortsBefore)) {
        if (-not [string]::IsNullOrWhiteSpace($name)) {
            $before[$name.Trim().ToUpperInvariant()] = $true
        }
    }
    while ((Get-Date) -lt $deadline) {
        $knownCandidates = New-Object 'System.Collections.Generic.List[string]'
        foreach ($name in @($PortName, $FallbackRuntimePortName)) {
            if (-not [string]::IsNullOrWhiteSpace($name) -and
                -not $knownCandidates.Contains($name.Trim().ToUpperInvariant())) {
                $knownCandidates.Add($name.Trim().ToUpperInvariant())
            }
        }

        # Check deterministic ports first. A fresh PnP inventory can be slow
        # when Windows is retiring the composite bootloader interfaces, and it
        # must not delay success when the selected runtime COM has returned.
        $fastRuntimeSnapshot = Get-NiusFastUsbSerialRegistrySnapshot `
            -Vid $RuntimeVid -ProductId $RuntimePid `
            -PreferredCompositeStableId $PreferredCompositeStableId
        foreach ($candidate in $knownCandidates) {
            # A bootloader CDC can retain the same COM number briefly after a
            # successful transfer. Require the runtime VID/PID before accepting
            # this deterministic name. Do not open the fresh application COM:
            # usbser.sys can block that probe while retiring the bootloader node
            # and then delay the user's first Serial Monitor connection.
            $runtimeIdentityMatches = $false
            if ($fastRuntimeSnapshot.Available) {
                $runtimeIdentityMatches = @($fastRuntimeSnapshot.Matches | Where-Object {
                        ([string]$_.DeviceID).Trim().ToUpperInvariant() -eq $candidate
                    }).Count -gt 0
            }
            else {
                $runtimeIdentityMatches =
                    -not [string]::IsNullOrWhiteSpace($RuntimeVid) -and
                    -not [string]::IsNullOrWhiteSpace($RuntimePid) -and
                    (Test-SerialPortMatchesUsbIdentity `
                        -PortName $candidate `
                        -Vid $RuntimeVid `
                        -ProductId $RuntimePid)
                $stableIdMatches = [string]::IsNullOrWhiteSpace($PreferredCompositeStableId) -or
                    ((Get-SerialPortUsbParentCompositeStableId -PortName $candidate -Fresh) -eq $PreferredCompositeStableId.Trim().ToUpperInvariant())
                $runtimeIdentityMatches = $runtimeIdentityMatches -and $stableIdMatches
            }
            if ($runtimeIdentityMatches) {
                Write-NiusTiming ('post-upload runtime verified: {0}' -f $candidate)
                return [pscustomobject]@{
                    Success = $true
                    Summary = ('post-upload runtime COM {0} has the expected identity' -f $candidate)
                }
            }
        }

        $remappedCandidates = New-Object 'System.Collections.Generic.List[string]'
        foreach ($name in @(Get-NiusRuntimeComNamesForIdentity `
                    -RuntimeVid $RuntimeVid -RuntimePid $RuntimePid `
                    -PreferredCompositeStableId $PreferredCompositeStableId -Fresh)) {
            $normalized = $name.Trim().ToUpperInvariant()
            # A runtime port that did not exist before this upload belongs to
            # the just-rebooted target even when its clone bootloader drops the
            # USB serial string and Windows assigns a different COM number.
            if (-not $before.ContainsKey($normalized) -and
                -not $knownCandidates.Contains($normalized) -and
                -not $remappedCandidates.Contains($normalized)) {
                $remappedCandidates.Add($normalized)
            }
        }

        foreach ($candidate in $remappedCandidates) {
            Write-NiusTiming ('post-upload remapped runtime verified: {0}' -f $candidate)
            return [pscustomobject]@{
                Success = $true
                Summary = ('post-upload runtime COM {0} has the expected identity' -f $candidate)
            }
        }

        Start-Sleep -Milliseconds 400
    }

    Write-NiusDetail '[nius] misflash guard: USB serial did not return after upload; attempting bootloader recovery...' -ForegroundColor Yellow
    $recovery = Invoke-NiusRecoverBoardToBootloader `
        -PortName $PortName `
        -ExpectedLabel $ExpectedLabel `
        -ExpectedModel $ExpectedModel `
        -ExpectedBoardId $ExpectedBoardId `
        -PreferredCompositeStableId $PreferredCompositeStableId `
        -BootloaderVid $BootloaderVid `
        -BootloaderPid $BootloaderPid

    $compiled = Normalize-NiusHexAddress -Value $ExpectedAppStart
    $lines = New-Object 'System.Collections.Generic.List[string]'
    $lines.Add('Upload finished, but the board USB serial never came back in application mode.')
    if (-not [string]::IsNullOrWhiteSpace($compiled)) {
        $lines.Add(('This sketch was compiled for app start {0}. A mismatch with the mounted bootloader (wrong Bootloader / DFU menu) often causes USB to disappear after flash.' -f $compiled))
    }
    $lines.Add('Select the matching Bootloader / DFU layout (no-SoftDevice clones: no SoftDevice / MBR only 0x1000), recompile, then upload again.')
    if ($recovery.Recovered) {
        $lines.Add(('Recovery: {0}. The board should be in UF2/DFU again - fix the menu option before the next upload.' -f $recovery.Detail))
    }
    else {
        $lines.Add(('Recovery failed ({0}). Double-tap RESET, re-plug USB, or use SWD/J-Link, then enter UF2/DFU manually.' -f $recovery.Detail))
    }
    $lines.Add('ZH: Upload finished but USB serial never returned - usually wrong Bootloader / DFU app start. For no-SoftDevice clones use 0x1000, recompile, upload again.')

    Throw-NiusUploadFailure (New-UploadFailure -Kind 'misflash' -ExitCode 1 -Output ($lines.ToArray() -join [Environment]::NewLine) -Exe $toolPath)
}

function Get-PnpVidPidMatches {
    param(
        [string]$VidLetters,
        [string]$PidLetters,
        [string]$InterfaceParentPrefix = '',
        [string]$PreferredCompositeStableId = ''
    )

    $needle = ('USB\VID_{0}&PID_{1}' -f $VidLetters, $PidLetters).ToUpperInvariant()
    $normalizedParentPrefix = $InterfaceParentPrefix.Trim().ToUpperInvariant()
    $normalizedStableId = $PreferredCompositeStableId.Trim().ToUpperInvariant()
    $candidates = @(Get-PnpDeviceInventory)
    return @($candidates | Where-Object {
        $instanceId = ([string]$_.InstanceId).ToUpperInvariant()
        $hardwareId = ([string](($_.HardwareID -join ';'))).ToUpperInvariant()
        $matchesVidPid = $instanceId.StartsWith($needle) -or ($hardwareId -match [Regex]::Escape(('VID_{0}&PID_{1}' -f $VidLetters, $PidLetters).ToUpperInvariant()))
        if (-not $matchesVidPid) {
            return $false
        }

        if (-not [string]::IsNullOrWhiteSpace($normalizedStableId)) {
            $deviceStableId = Get-UsbInterfaceParentCompositeStableId -PnpInstanceId $instanceId
            if ([string]::IsNullOrWhiteSpace($deviceStableId)) {
                return $false
            }

            return $deviceStableId.Trim().ToUpperInvariant() -eq $normalizedStableId
        }

        if (-not [string]::IsNullOrWhiteSpace($normalizedParentPrefix)) {
            $deviceParentPrefix = Get-UsbInterfaceParentInstancePrefix -PnpInstanceId $instanceId
            return (-not [string]::IsNullOrWhiteSpace($deviceParentPrefix)) -and $deviceParentPrefix -eq $normalizedParentPrefix
        }

        return $true
    })
}

function Format-PnpMatchSummary {
    param([object[]]$Matches)

    if (-not $Matches -or $Matches.Count -eq 0) {
        return '(no matching PnP nodes present)'
    }

    return (($Matches | ForEach-Object {
        $friendly = if ([string]::IsNullOrWhiteSpace($_.FriendlyName)) { '<unnamed>' } else { $_.FriendlyName }
        ('{0} [{1}]' -f $friendly, $_.InstanceId)
    }) -join '; ')
}

function Get-AdafruitRuntimeSnapshot {
    param(
        [string]$BootloaderVid,
        [string]$BootloaderPid,
        [string]$RuntimeVid = '',
        [string]$RuntimePid = '',
        [string]$InterfaceParentPrefix = '',
        [string]$PreferredCompositeStableId = ''
    )

    $vidLetters = $BootloaderVid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $pidLetters = $BootloaderPid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $matches = @(Get-PnpVidPidMatches -VidLetters $vidLetters -PidLetters $pidLetters -InterfaceParentPrefix $InterfaceParentPrefix -PreferredCompositeStableId $PreferredCompositeStableId)
    # MI_02 is MSC in the bootloader but user CDC in our dual-CDC runtime.
    # Exclude Ports/Modem class devices so that user CDC on MI_02 is not
    # incorrectly counted as a storage (MSC/UF2) interface.
    #
    # usbcdc=disabled has NO user CDC, so the runtime DFU "Bootloader Control"
    # vendor interface lands on MI_02 (where usbcdc=enabled puts the user CDC).
    # It is a WinUSB/vendor class (not Ports), so without the FriendlyName guard
    # below it was miscounted as MSC -> strongBootloaderEvidence -> upload.ps1
    # decided the board was ALREADY in the bootloader, SKIPPED the 1200 touch,
    # and ran adafruit-nrfutil against the still-running app (it stalled). The
    # real bootloader is recognized by its UF2 mass-storage VOLUME
    # (Get-Uf2ProbeSummary), never by this control interface, so excluding it by
    # name is safe.
    $storageInterfaces = @($matches | Where-Object {
        ([string]$_.InstanceId).ToUpperInvariant() -like '*&MI_02*' -and
        ([string]$_.Class).ToUpperInvariant() -notin @('PORTS', 'MODEM') -and
        ([string]$_.FriendlyName).ToUpperInvariant() -notlike '*BOOTLOADER CONTROL*'
    })
    $cdcInterfaces = @($matches | Where-Object { ([string]$_.InstanceId).ToUpperInvariant() -like '*&MI_00*' })
    $composites = @($matches | Where-Object { ([string]$_.InstanceId).ToUpperInvariant() -notlike '*&MI_*' })

    $runtimePresent = $false
    $runtimeSummary = 'runtime=untracked'
    if (-not [string]::IsNullOrWhiteSpace($RuntimeVid) -and -not [string]::IsNullOrWhiteSpace($RuntimePid)) {
        $runtimeVidLetters = $RuntimeVid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
        $runtimePidLetters = $RuntimePid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
        $runtimeMatches = @(Get-PnpVidPidMatches -VidLetters $runtimeVidLetters -PidLetters $runtimePidLetters -InterfaceParentPrefix $InterfaceParentPrefix -PreferredCompositeStableId $PreferredCompositeStableId)
        $runtimePresent = $runtimeMatches.Count -gt 0
        $runtimeSummary = ('runtime_matches={0}; runtime_nodes={1}' -f $runtimeMatches.Count, (Format-PnpMatchSummary -Matches $runtimeMatches))
    }

    return [pscustomobject]@{
        BootloaderPresent = $matches.Count -gt 0
        RuntimePresent = $runtimePresent
        CompositeCount = $composites.Count
        CdcInterfaceCount = $cdcInterfaces.Count
        StorageInterfaceCount = $storageInterfaces.Count
        Summary = ('matches={0}; composite={1}; cdc={2}; msc={3}; nodes={4}; {5}' -f $matches.Count, $composites.Count, $cdcInterfaces.Count, $storageInterfaces.Count, (Format-PnpMatchSummary -Matches $matches), $runtimeSummary)
    }
}

function Get-Uf2MassStorageProblemReports {
    param(
        [string]$UsbVid,
        [string]$UsbPid,
        [string]$PreferredCompositeStableId = '',
        [string]$InterfaceParentPrefix = ''
    )

    if ([string]::IsNullOrWhiteSpace($UsbVid) -or
        [string]::IsNullOrWhiteSpace($UsbPid) -or
        $UsbVid -eq 'auto' -or
        $UsbPid -eq 'auto') {
        return @()
    }

    $vidLetters = $UsbVid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $pidLetters = $UsbPid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $needle = ('USB\VID_{0}&PID_{1}&MI_02\' -f $vidLetters, $pidLetters).ToUpperInvariant()
    $preferredStable = ([string]$PreferredCompositeStableId).Trim().ToUpperInvariant()
    $preferredPrefix = ([string]$InterfaceParentPrefix).Trim().ToUpperInvariant()
    $reports = New-Object 'System.Collections.Generic.List[string]'

    # Problem code + bound driver for every device in ONE CIM query (~1 s) instead
    # of four Get-PnpDeviceProperty calls per matching device (~2.3 s each on a
    # busy host). ConfigManagerErrorCode is the problem code (0 = OK).
    $cimByInstance = @{}
    try {
        foreach ($e in @(Get-CimInstance -ClassName Win32_PnPEntity -ErrorAction Stop)) {
            $did = ([string]$e.DeviceID).Trim().ToUpperInvariant()
            if ($did -and -not $cimByInstance.ContainsKey($did)) { $cimByInstance[$did] = $e }
        }
    }
    catch {
    }

    foreach ($device in @(Get-PnpDeviceInventory)) {
        $instanceId = ([string]$device.InstanceId).Trim()
        $upperInstanceId = $instanceId.ToUpperInvariant()
        if (-not $upperInstanceId.StartsWith($needle)) {
            continue
        }

        $parentPrefix = Get-UsbInterfaceParentInstancePrefix -PnpInstanceId $instanceId
        if (-not [string]::IsNullOrWhiteSpace($preferredPrefix) -and $parentPrefix -ne $preferredPrefix) {
            continue
        }

        $stableId = Get-UsbInterfaceParentCompositeStableId -PnpInstanceId $instanceId
        if (-not [string]::IsNullOrWhiteSpace($preferredStable) -and ([string]$stableId).Trim().ToUpperInvariant() -ne $preferredStable) {
            continue
        }

        $cimEntity = $cimByInstance[$upperInstanceId]
        $problemCode = if ($cimEntity) { $cimEntity.ConfigManagerErrorCode } else { $null }
        $service = if ($cimEntity) { [string]$cimEntity.Service } else { '' }
        $problemStatus = 'n/a'   # diagnostic-only; not needed for the decision
        $driverInf = 'n/a'       # (kept fast - no per-property PnP query)

        $hasProblem = $false
        if ($null -ne $problemCode) {
            $hasProblem = ([int]$problemCode -ne 0)
        }
        if (-not $hasProblem -and ([string]$device.Status).Trim().ToUpperInvariant() -ne 'OK') {
            $hasProblem = $true
        }
        if (-not $hasProblem) {
            continue
        }

        $reports.Add(('UF2 MSC interface is present but Windows did not start it: id="{0}"; status={1}; problem_code={2}; problem_status={3}; service={4}; driver={5}; stable_id={6}.' -f $instanceId, $device.Status, $problemCode, $problemStatus, $service, $driverInf, $stableId))
    }

    return $reports.ToArray()
}

function Invoke-Uf2SerialDfuFallback {
    param(
        [string]$HexPath,
        [string]$SelectedPort,
        [string]$CurrentPort,
        [string]$BootloaderVid,
        [string]$BootloaderPid,
        [string]$PreferredCompositeStableId = '',
        [string]$InterfaceParentPrefix = '',
        [string]$SdReq = '',
        [string]$ExpectedAppStart = '',
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = ''
    )

    if ($env:NIUS_DISABLE_UF2_SERIAL_FALLBACK -eq '1') {
        return $false
    }

    $portResolution = Resolve-AdafruitBootloaderControlPort `
        -SelectedPort $SelectedPort `
        -CurrentPort $CurrentPort `
        -BootloaderVid $BootloaderVid `
        -BootloaderPid $BootloaderPid `
        -PreferredCompositeStableId $PreferredCompositeStableId `
        -InterfaceParentPrefix $InterfaceParentPrefix `
        -Fresh

    if (-not $portResolution -or [string]::IsNullOrWhiteSpace($portResolution.Port)) {
        return $false
    }

    $fallbackPort = $portResolution.Port.Trim()
    if ([string]::IsNullOrWhiteSpace($fallbackPort) -or $fallbackPort.StartsWith('{')) {
        return $false
    }

    $problems = @(Get-Uf2MassStorageProblemReports -UsbVid $BootloaderVid -UsbPid $BootloaderPid -PreferredCompositeStableId $PreferredCompositeStableId -InterfaceParentPrefix $InterfaceParentPrefix)
    foreach ($problem in $problems) {
        Write-NiusDetail ('[nius] {0}' -f $problem) -ForegroundColor DarkYellow
    }

    Write-NiusDetail ('[nius] UF2 volume is unavailable; using bootloader serial DFU on {0} ({1}).' -f $fallbackPort, $portResolution.Reason) -ForegroundColor DarkYellow
    Invoke-NiusPreUploadLayoutGuard `
        -ExpectedAppStart $ExpectedAppStart `
        -Context 'UF2 serial fallback pre-flash' `
        -ExpectedLabel $ExpectedLabel `
        -ExpectedModel $ExpectedModel `
        -ExpectedBoardId $ExpectedBoardId `
        -PreferredCompositeStableId $PreferredCompositeStableId `
        -RequireUf2Evidence `
        -Uf2ProbeWaitMs 12000 `
        -PortName $fallbackPort
    $initialSdReq = Resolve-AdafruitInitialSdReq -SdReq $SdReq
    Invoke-AdafruitDfuDeploy -HexPath $HexPath -Port $fallbackPort -SdReq $initialSdReq
    return $true
}

function Wait-ForAdafruitRuntimeTransition {
    param(
        [string]$BootloaderVid,
        [string]$BootloaderPid,
        [string]$RuntimeVid = '',
        [string]$RuntimePid = '',
        [string]$InterfaceParentPrefix = '',
        [string]$PreferredCompositeStableId = '',
        # Poll every 150 ms (was 500); Attempts scaled to keep the ~120 s timeout.
        # This also shrinks the 2-stable-detection confirm from ~1 s to ~300 ms, so
        # the post-flash "rebooted into new firmware" step finishes sooner.
        [int]$Attempts = 800,
        [int]$DelayMs = 150
    )

    $stableRuntimeDetections = 0
    $lastSnapshot = $null
    for ($attempt = 0; $attempt -lt $Attempts; $attempt++) {
        $lastSnapshot = Get-AdafruitRuntimeSnapshot -BootloaderVid $BootloaderVid -BootloaderPid $BootloaderPid -RuntimeVid $RuntimeVid -RuntimePid $RuntimePid -InterfaceParentPrefix $InterfaceParentPrefix -PreferredCompositeStableId $PreferredCompositeStableId
        # Device disappearance is not application success. A caller without an
        # explicit runtime identity cannot prove that the uploaded app started.
        $runtimeSatisfied = -not [string]::IsNullOrWhiteSpace($RuntimeVid) -and
            -not [string]::IsNullOrWhiteSpace($RuntimePid) -and
            $lastSnapshot.RuntimePresent
        if (-not $lastSnapshot.BootloaderPresent -and $runtimeSatisfied) {
            $stableRuntimeDetections += 1
            if ($stableRuntimeDetections -ge 2) {
                return [pscustomobject]@{
                    Success = $true
                    Summary = $lastSnapshot.Summary
                }
            }
        } else {
            $stableRuntimeDetections = 0
        }

        Start-Sleep -Milliseconds $DelayMs
    }

    return [pscustomobject]@{
        Success = $false
        Summary = if ($lastSnapshot) { $lastSnapshot.Summary } else { 'no post-upload USB snapshot captured' }
    }
}

function Resolve-AutoBootloader {
    param(
        [string]$InterfaceParentPrefix = '',
        [string]$PreferredCompositeStableId = '',
        [int]$Attempts = 4,
        [int]$DelayMs = 400
    )

    $probedNames = ($script:NrfBootloaderCandidates | ForEach-Object { ('{0}:{1}' -f $_.Vid, $_.Pid) }) -join ', '

    for ($attempt = 0; $attempt -lt $Attempts; $attempt++) {
        # A mounted INFO_UF2 volume already provides the strongest available
        # evidence: board identity, layout metadata, and the deployment path.
        # Check it before the much slower full Windows PnP inventory. The
        # preferred composite ID keeps this scoped to the board that was just
        # touched even when several UF2-capable boards are connected.
        $uf2Any = Get-Uf2ProbeSummary -PreferredCompositeStableId $PreferredCompositeStableId
        if ($uf2Any) {
            $layout = Resolve-Uf2FlashLayout -Uf2Summary $uf2Any -DefaultAppStart '0x26000' -DefaultMaxSize 876544 -DefaultNote 'UF2 volume identified by INFO_UF2.TXT only'
            Write-NiusDetail ('[nius] auto-detect: matched UF2 volume {0} (label={1}, model={2}); VID/PID unknown, treating as Adafruit-fork UF2; {3}' -f $uf2Any.Drive, $uf2Any.Label, $uf2Any.Model, $layout.Note)
            return [pscustomobject]@{
                Resolved = $true
                Source = 'uf2-volume'
                Vid = '0x239A'
                Pid = '0x00B3'
                Kind = 'uf2'
                Family = '0xADA52840'
                AppStart = $layout.AppStart
                MaxSize = $layout.MaxSize
                Note = $layout.Note
                VolumeLabel = $uf2Any.Label
                Model = $uf2Any.Model
                BoardId = $uf2Any.BoardId
                DriveRoot = $uf2Any.Drive
            }
        }

        $pnpInventory = @(Get-PnpDeviceInventory)

        function Test-PnpVidPidInSnapshot {
            param(
                [object[]]$Inventory,
                [string]$VidLetters,
                [string]$PidLetters,
                [string]$ExpectedCompositeStableId = ''
            )

            $needle = ('VID_{0}&PID_{1}' -f $VidLetters, $PidLetters).ToUpperInvariant()
            foreach ($device in @($Inventory)) {
                $instanceId = ([string]$device.InstanceId).ToUpperInvariant()
                $hwId = ([string](($device.HardwareID -join ';'))).ToUpperInvariant()
                if (-not ($instanceId.Contains($needle) -or $hwId.Contains($needle))) {
                    continue
                }
                if (-not [string]::IsNullOrWhiteSpace($ExpectedCompositeStableId)) {
                    $candidateStableId = Get-UsbInterfaceParentCompositeStableId -PnpInstanceId $instanceId
                    if ($candidateStableId -ne $ExpectedCompositeStableId.Trim().ToUpperInvariant()) { continue }
                }
                return $true
            }
            return $false
        }

        foreach ($candidate in $script:NrfBootloaderCandidates) {
            $needle = ('{0}:{1}' -f $candidate.Vid, $candidate.Pid)
            $hitPnp = Test-PnpVidPidInSnapshot -Inventory $pnpInventory -VidLetters $candidate.Vid -PidLetters $candidate.Pid -ExpectedCompositeStableId $PreferredCompositeStableId
            if ($hitPnp) {
                $expectedLabel = if ($candidate.ContainsKey('VolumeLabel')) { $candidate.VolumeLabel } else { '' }
                $expectedModel = if ($candidate.ContainsKey('Model')) { $candidate.Model } else { '' }
                $expectedBoardId = if ($candidate.ContainsKey('BoardId')) { $candidate.BoardId } else { '' }
                $uf2 = $null
                if (-not [string]::IsNullOrWhiteSpace($candidate.Family) -or $candidate.Kind -eq 'uf2') {
                    $uf2 = Get-Uf2ProbeSummary -ExpectedLabel $expectedLabel -ExpectedModel $expectedModel -ExpectedBoardId $expectedBoardId -PreferredCompositeStableId $PreferredCompositeStableId
                }

                if (-not [string]::IsNullOrWhiteSpace($InterfaceParentPrefix) -and $candidate.Kind -eq 'adafruit-dfu') {
                    $scopedSnapshot = Get-AdafruitRuntimeSnapshot -BootloaderVid ('0x{0}' -f $candidate.Vid) -BootloaderPid ('0x{0}' -f $candidate.Pid) -InterfaceParentPrefix $InterfaceParentPrefix
                    $scopedBootloaderEvidence = ($scopedSnapshot.StorageInterfaceCount -gt 0) -or ($null -ne $uf2)
                    if (-not $scopedBootloaderEvidence) {
                        Write-NiusDetail ('[nius] auto-detect: ignoring PnP-only same-PID match {0} on selected board scope {1}; no MSC/UF2 bootloader evidence ({2})' -f $needle, $InterfaceParentPrefix, $scopedSnapshot.Summary) -ForegroundColor DarkGray
                        continue
                    }
                }

                $resolvedKind = if ($uf2) { 'uf2' } else { $candidate.Kind }
                $layout = Resolve-Uf2FlashLayout -Uf2Summary $uf2 -DefaultAppStart $candidate.AppStart -DefaultMaxSize $candidate.MaxSize -DefaultNote $candidate.Note
                Write-NiusDetail ('[nius] auto-detect: matched {0} via scoped PnP -- {1}{2}' -f $needle, $layout.Note, $(if ($uf2) { '; mounted UF2 volume preferred' } else { '' }))
                return [pscustomobject]@{
                    Resolved = $true
                    Source = 'pnp'
                    Vid = ('0x{0}' -f $candidate.Vid)
                    Pid = ('0x{0}' -f $candidate.Pid)
                    Kind = $resolvedKind
                    Family = $candidate.Family
                    AppStart = $layout.AppStart
                    MaxSize = $layout.MaxSize
                    Note = $layout.Note
                    VolumeLabel = if ($uf2) { $uf2.Label } elseif ($candidate.ContainsKey('VolumeLabel')) { $candidate.VolumeLabel } else { '' }
                    Model = if ($uf2) { $uf2.Model } elseif ($candidate.ContainsKey('Model')) { $candidate.Model } else { '' }
                    BoardId = if ($uf2) { $uf2.BoardId } elseif ($candidate.ContainsKey('BoardId')) { $candidate.BoardId } else { '' }
                    DriveRoot = if ($uf2) { $uf2.Drive } else { '' }
                }
            }
        }

        Start-Sleep -Milliseconds $DelayMs
    }

    return [pscustomobject]@{
        Resolved = $false
        ProbedCandidates = $probedNames
    }
}

function Invoke-Uf2Deploy {
    param(
        [string]$HexPath,
        [string]$FamilyId,
        [string]$AppStart,
        [string]$MaxSize,
        [string]$RamEnd,
        [string]$DrivePath
    )

    $uf2Path = $null
    try {
        Write-NiusTiming 'UF2 conversion start'
        $uf2Path = New-Uf2Artifact `
            -HexPath $HexPath `
            -FamilyId $FamilyId `
            -AppStart $AppStart `
            -MaxSize $MaxSize `
            -RamEnd $RamEnd
        Write-NiusTiming 'UF2 conversion done'
        $destination = Join-Path $DrivePath ([System.IO.Path]::GetFileName($uf2Path))
        Write-NiusTiming 'UF2 copy start'
        Copy-Item -LiteralPath $uf2Path -Destination $destination -Force
        Write-NiusTiming 'UF2 copy returned'
    }
    finally {
        if (-not [string]::IsNullOrWhiteSpace($uf2Path) -and [System.IO.File]::Exists($uf2Path)) {
            try { [System.IO.File]::Delete($uf2Path) } catch { }
        }
    }
}

function Test-NiusResolvedPathUnderConda {
    param([string]$FullPath)

    if ([string]::IsNullOrWhiteSpace($FullPath)) {
        return $false
    }
    $lower = $FullPath.ToLowerInvariant().Replace('/', '\')
    foreach ($frag in @(
        '\anaconda\',
        '\anaconda3\',
        '\miniconda\',
        '\miniconda3\',
        '\mambaforge\',
        '\micromamba\',
        '\conda\pkgs\',
        '\condabin\'
    )) {
        if ($lower.Contains($frag)) {
            return $true
        }
    }
    return $false
}

function Resolve-AdafruitNrfutil {
    # Highest priority: the Boards-Manager-bundled binary passed by
    # platform.txt via -NrfutilExe (no Python / Conda needed on the host).
    if (-not [string]::IsNullOrWhiteSpace($NrfutilExe)) {
        $bundled = $NrfutilExe.Trim().Trim('"')
        if (Test-Path -LiteralPath $bundled) {
            return (Resolve-Path -LiteralPath $bundled).Path
        }
    }

    # Machine-agnostic adafruit-nrfutil discovery. No developer-specific
    # paths: we look at an explicit override, then PATH, then the standard
    # python.org per-user Scripts directories. Conda installs are accepted
    # only as a last resort (their bundled nordicsemi often mismatches the
    # serial DFU protocol) and can be force-allowed via env var.
    if (-not [string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_NRFUTIL_EXE)) {
        $explicit = $env:NIUS_ADAFRUIT_NRFUTIL_EXE.Trim().Trim('"')
        if (Test-Path -LiteralPath $explicit) {
            return (Resolve-Path -LiteralPath $explicit).Path
        }
    }

    $ordered = New-Object System.Collections.Generic.List[string]

    # python.org per-user installs (most common non-conda layout on Windows).
    foreach ($ver in @('313', '312', '311', '310', '39')) {
        $cand = "$env:LOCALAPPDATA\Programs\Python\Python$ver\Scripts\adafruit-nrfutil.exe"
        if (Test-Path -LiteralPath $cand) {
            $rp = (Resolve-Path -LiteralPath $cand).Path
            if (-not $ordered.Contains($rp)) { [void]$ordered.Add($rp) }
        }
    }

    # Anything on PATH (where.exe enumerates every hit in order).
    try {
        foreach ($line in @(& where.exe adafruit-nrfutil 2>$null)) {
            $t = ([string]$line).Trim().Trim('"')
            if ([string]::IsNullOrWhiteSpace($t) -or -not (Test-Path -LiteralPath $t)) { continue }
            $rp = (Resolve-Path -LiteralPath $t).Path
            if (-not $ordered.Contains($rp)) { [void]$ordered.Add($rp) }
        }
    }
    catch {
    }

    foreach ($gc in @(Get-Command 'adafruit-nrfutil' -ErrorAction SilentlyContinue -All)) {
        if (-not $gc.Source -or -not (Test-Path -LiteralPath $gc.Source)) { continue }
        $rp = (Resolve-Path -LiteralPath $gc.Source).Path
        if (-not $ordered.Contains($rp)) { [void]$ordered.Add($rp) }
    }

    if ($ordered.Count -eq 0) {
        return $null
    }

    # Prefer the first non-conda hit.
    foreach ($p in $ordered) {
        if (-not (Test-NiusResolvedPathUnderConda $p)) {
            return $p
        }
    }

    # Only Conda installs are available. Do not silently accept a potentially
    # incompatible protocol implementation. An explicit executable path remains
    # available for operators who have independently verified another build.
    Write-NiusDetail '[nius] Only a Conda adafruit-nrfutil was found; refusing an implicit protocol-tool substitution. Use the bundled tool or set NIUS_ADAFRUIT_NRFUTIL_EXE to an independently verified executable.' -ForegroundColor Yellow
    return $null
}

function Stop-NiusLingeringAdafruitNrfutil {
    param(
        [ValidateSet('touch', 'dfu')]
        [string]$Phase = 'touch'
    )

    # Never kill by image name or infer ownership from a missing parent. Another
    # IDE/tool may legitimately have launched the process, and a parent can exit
    # after intentionally handing work to a child. Only Stop-NiusProcessTree may
    # terminate the exact PID tree started by this uploader. Any pre-existing
    # nrfutil is an external owner and fails closed without touching it.
    $processes = @(Get-CimInstance Win32_Process -Filter "Name='adafruit-nrfutil.exe'" -ErrorAction SilentlyContinue)
    $lingering = @($processes)
    if ($lingering.Count -eq 0) {
        return
    }

    $owners = ($lingering | ForEach-Object {
            'pid={0}, parent={1}' -f $_.ProcessId, $_.ParentProcessId
        }) -join '; '
    throw ('A pre-existing adafruit-nrfutil process may own a board or serial endpoint ({0}). ArduinoNRF left every process untouched; wait for that operation to finish.' -f $owners)
}

function Normalize-NiusUsbId {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ''
    }
    $normalized = $Value.Trim().Replace('0x', '').Replace('0X', '').TrimStart([char]'0').ToUpperInvariant()
    if ([string]::IsNullOrWhiteSpace($normalized)) {
        return '0'
    }
    return $normalized
}

function Stop-NiusProcessTree {
    param([int]$RootPid)

    if ($RootPid -le 0) {
        return
    }

    $all = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue)
    $childrenByParent = @{}
    foreach ($proc in $all) {
        $parentId = [int]$proc.ParentProcessId
        if (-not $childrenByParent.ContainsKey($parentId)) {
            $childrenByParent[$parentId] = New-Object System.Collections.Generic.List[int]
        }
        $childrenByParent[$parentId].Add([int]$proc.ProcessId)
    }

    $pending = New-Object System.Collections.Generic.Queue[int]
    $pending.Enqueue($RootPid)
    $killOrder = New-Object System.Collections.Generic.List[int]

    while ($pending.Count -gt 0) {
        $parentId = $pending.Dequeue()
        if (-not $childrenByParent.ContainsKey($parentId)) {
            continue
        }
        foreach ($childId in $childrenByParent[$parentId]) {
            if ($childId -eq $PID -or $killOrder.Contains($childId)) {
                continue
            }
            $killOrder.Add($childId)
            $pending.Enqueue($childId)
        }
    }

    [Array]::Reverse($killOrder)
    foreach ($childId in $killOrder) {
        try {
            Stop-Process -Id $childId -Force -ErrorAction Stop
        }
        catch {
        }
    }

    try {
        Stop-Process -Id $RootPid -Force -ErrorAction Stop
    }
    catch {
    }
}

function Resolve-AdafruitInitialSdReq {
    param(
        [string]$SdReq
    )

    if ([string]::IsNullOrWhiteSpace($SdReq)) {
        throw 'The selected serial-DFU layout does not declare an sd-req; refusing to infer a SoftDevice generation.'
    }

    $normalizedSdReq = $SdReq.Trim().ToUpperInvariant()
    if ($normalizedSdReq -notmatch '^0X[0-9A-F]{1,4}$') {
        throw ('Invalid serial-DFU sd-req: {0}' -f $SdReq)
    }
    return ('0x{0}' -f $normalizedSdReq.Substring(2))
}

function Get-NiusAdafruitSerialReadyMilliseconds {
    $milliseconds = 12000
    if (-not [string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS)) {
        $configured = -1
        if ([int]::TryParse($env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS, [ref]$configured) -and
            $configured -ge 1000 -and $configured -le 120000) {
            $milliseconds = $configured
        }
    }
    return $milliseconds
}

function Invoke-AdafruitDfuDeploy {
    param(
        [string]$HexPath,
        [string]$Port,
        [string]$DevType = '0x0052',
        [int]$BaudRate = 115200,
        [string]$SdReq = ''
    )

    if ([string]::IsNullOrWhiteSpace($HexPath) -or -not (Test-Path -LiteralPath $HexPath)) {
        throw ('Compiled hex file not found for adafruit-dfu upload: {0}' -f $HexPath)
    }
    if ([string]::IsNullOrWhiteSpace($Port) -or $Port.StartsWith('{')) {
        throw 'adafruit-dfu upload requires a concrete serial port name in -Port; got an unresolved placeholder. The Arduino IDE Tools > Port selection has not been made.'
    }
    $tool = Resolve-AdafruitNrfutil
    if (-not $tool) {
        throw 'adafruit-nrfutil was not found. Install with non-Conda Python (`pip install adafruit-nrfutil`) or set NIUS_ADAFRUIT_NRFUTIL_EXE to an independently verified executable. UF2 drag-drop is also supported via the UF2 bootloader menu entries.'
    }
    Write-NiusDetail ('[nius] Resolved adafruit-nrfutil: {0}' -f $tool) -ForegroundColor DarkGray

    Write-NiusDetail '[nius] Adafruit DFU pipeline starting (genpkg, then serial)...' -ForegroundColor DarkGray

    Stop-NiusLingeringAdafruitNrfutil -Phase dfu

    # Package generation is invocation-owned. Do not overwrite or leave a ZIP
    # beside the build HEX: a later upload must never consume a stale package.
    $zipPath = Join-Path ([System.IO.Path]::GetTempPath()) `
        ('arduinonrf-dfu-{0}.zip' -f ([Guid]::NewGuid().ToString('n')))

    try {
        $resolvedSdReq = Resolve-AdafruitInitialSdReq -SdReq $SdReq

        $genpkgArgs = @('dfu', 'genpkg', '--dev-type', $DevType)
        if (-not [string]::IsNullOrWhiteSpace($resolvedSdReq)) {
            $genpkgArgs += @('--sd-req', $resolvedSdReq)
        }
        $genpkgArgs += @('--application', $HexPath, $zipPath)

        Write-NiusTiming 'deploy-enter (genpkg start)'
        if ($script:NiusVerbose) { Write-Stage -Percent 55 -Label 'Generating Adafruit DFU package (genpkg)' }
        Invoke-CommandChecked -Exe $tool -Arguments $genpkgArgs -FailureKind 'adafruit-genpkg' -ProgressPercent 60 -ProgressLabel 'genpkg synthesizing DFU package'

        if (-not (Test-Path -LiteralPath $zipPath)) {
            throw ('adafruit-nrfutil genpkg did not produce expected output: {0}' -f $zipPath)
        }
        Write-NiusTiming 'genpkg-done'

        try {
            $serialReadyMs = Get-NiusAdafruitSerialReadyMilliseconds
            Wait-SerialPortReady -PortName $Port -Purpose 'Adafruit DFU control port' -TimeoutMs $serialReadyMs
        } catch {
            Throw-NiusUploadFailure (New-UploadFailure -Kind 'adafruit-dfu' -ExitCode 1 -Output $_.Exception.Message -Exe $tool)
        }
        Write-NiusTiming 'serial-ready'

        # Derive the real firmware size so the progress bar can track transferred
        # bytes instead of guessing. adafruit-nrfutil sends one HCI data frame per
        # DFU_PACKET_MAX_SIZE (512 B) chunk and echoes one '#' per frame, so
        # frames = ceil(bytes / 512). A '#' count / frame count gives true %.
        $fw = Get-NiusDfuFirmwareInfo -ZipPath $zipPath
        $totalFrames = if ($fw) { [int]$fw.Frames } else { 0 }
        $fwBytes = if ($fw) { [int]$fw.Bytes } else { 0 }

        # Visible transfer start: byte bar begins at 0% here, then advances as
        # nrfutil streams '#' frame markers (see Invoke-CommandChecked byteProgress).
        $startDetail = if ($fwBytes -gt 0) { '0.0/{0} KB' -f [Math]::Round($fwBytes / 1024.0, 1) } else { '' }
        Write-Stage -Percent 0 -Label 'Uploading' -Detail $startDetail
        Write-NiusTiming 'transfer-start'
        Invoke-CommandChecked -Exe $tool -Arguments @('--verbose', 'dfu', 'serial', '-pkg', $zipPath, '-p', $Port, '-b', [string]$BaudRate, '-sb') -FailureKind 'adafruit-dfu' -ProgressPercent 90 -ProgressLabel 'Uploading' -TotalFrames $totalFrames -FirmwareBytes $fwBytes
        Write-NiusTiming 'transfer-done'
    }
    finally {
        if ([System.IO.File]::Exists($zipPath)) {
            try { [System.IO.File]::Delete($zipPath) } catch { }
        }
    }
}

function Get-NiusDfuFirmwareInfo {
    # Reads the application .bin entry from a genpkg DFU .zip and returns the
    # firmware byte size and the number of 512 B DFU frames it will take.
    # Returns $null if the zip can't be inspected (callers fall back to a
    # frameless / time-based bar).
    param([string]$ZipPath)

    if ([string]::IsNullOrWhiteSpace($ZipPath) -or -not (Test-Path -LiteralPath $ZipPath)) {
        return $null
    }
    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
        $zip = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
        try {
            $bin = $zip.Entries | Where-Object { $_.FullName -match '\.bin$' } | Select-Object -First 1
            if (-not $bin) { return $null }
            $bytes = [int]$bin.Length
            if ($bytes -le 0) { return $null }
            $frames = [int][Math]::Ceiling($bytes / 512.0)
            return [pscustomobject]@{ Bytes = $bytes; Frames = $frames }
        }
        finally {
            $zip.Dispose()
        }
    }
    catch {
        return $null
    }
}

function Invoke-CommandChecked {
    param(
        [string]$Exe,
        [string[]]$Arguments,
        [string]$FailureKind = 'generic',
        [int]$ProgressPercent = 50,
        [string]$ProgressLabel = 'Working',
        [int]$TotalFrames = 0,
        [int]$FirmwareBytes = 0
    )

    # When TotalFrames > 0 the caller is the Adafruit serial-DFU transfer and
    # we render a real byte-based bar driven by the '#' frame markers nrfutil
    # streams. Updates are throttled to 20% milestones so the IDE console gets
    # ~6 lines (0/20/40/60/80/100) rather than a flood.
    $byteProgress = ($FailureKind -eq 'adafruit-dfu' -and $TotalFrames -gt 0)
    # Start at band 0: the caller already printed the 0% "Uploading" line, so
    # the first bar emitted here is the first non-zero 20% milestone.
    $lastShownBand = 0
    $fwKb = [Math]::Round($FirmwareBytes / 1024.0, 1)

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = ConvertTo-ProcessArguments -Arguments $Arguments
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()

    $tick = 0
    $timedOut = $false
    $timedOutIdle = $false
    $configuredTimeoutMs = 0
    $deadline = $null
    $idleMs = 0

    if ($FailureKind -eq 'adafruit-dfu' -or $FailureKind -eq 'adafruit-genpkg') {
        $configuredTimeoutMs = 240000
        $override = $env:NIUS_ADAFRUIT_DFU_PROCESS_TIMEOUT_MS
        if (-not [string]::IsNullOrWhiteSpace($override)) {
            $parsedTimeout = -1
            if ([int]::TryParse($override, [ref]$parsedTimeout) -and
                $parsedTimeout -ge 10000 -and $parsedTimeout -le 600000) {
                $configuredTimeoutMs = $parsedTimeout
            }
        }
        if ($configuredTimeoutMs -gt 0) {
            $deadline = (Get-Date).AddMilliseconds($configuredTimeoutMs)
        }

        # nrfutil often goes quiet during serial DFU payload phases, but leaving the
        # default too long makes buttonless same-COM retries sit at ~90% for an
        # unnecessarily long time before surfacing the real error.
        if ($FailureKind -eq 'adafruit-dfu') {
            $idleMs = 30000
            $idleOverride = $env:NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS
            if (-not [string]::IsNullOrWhiteSpace($idleOverride)) {
                $parsedIdle = -1
                if ([int]::TryParse($idleOverride, [ref]$parsedIdle) -and
                    $parsedIdle -ge 1000 -and $parsedIdle -le 120000) {
                    $idleMs = $parsedIdle
                }
            }
        }
    }
    elseif ($FailureKind -eq 'jlink') {
        # SEGGER J-Link Commander has been observed to keep its process alive
        # after a failed nRF52 flash transaction while Arduino IDE stays at the
        # last progress line. Treat that as a failed upload instead of letting a
        # bootloader/app write appear to hang forever.
        $configuredTimeoutMs = 120000
        $override = $env:NIUS_JLINK_PROCESS_TIMEOUT_MS
        if (-not [string]::IsNullOrWhiteSpace($override)) {
            $parsedTimeout = -1
            if ([int]::TryParse($override, [ref]$parsedTimeout) -and
                $parsedTimeout -ge 1000 -and $parsedTimeout -le 600000) {
                $configuredTimeoutMs = $parsedTimeout
            }
        }
        if ($configuredTimeoutMs -gt 0) {
            $deadline = (Get-Date).AddMilliseconds($configuredTimeoutMs)
        }
    }
    elseif ($FailureKind -eq 'openocd') {
        $configuredTimeoutMs = 120000
        $override = $env:NIUS_OPENOCD_PROCESS_TIMEOUT_MS
        if (-not [string]::IsNullOrWhiteSpace($override)) {
            $parsedTimeout = -1
            if ([int]::TryParse($override, [ref]$parsedTimeout) -and
                $parsedTimeout -ge 1000 -and $parsedTimeout -le 600000) {
                $configuredTimeoutMs = $parsedTimeout
            }
        }
        if ($configuredTimeoutMs -gt 0) {
            $deadline = (Get-Date).AddMilliseconds($configuredTimeoutMs)
        }
    }
    elseif ($FailureKind -eq 'uf2-convert' -or $FailureKind -eq 'image-preflight') {
        $configuredTimeoutMs = 60000
        if (-not [string]::IsNullOrWhiteSpace($env:NIUS_LOCAL_TOOL_PROCESS_TIMEOUT_MS)) {
            $parsedTimeout = -1
            if ([int]::TryParse($env:NIUS_LOCAL_TOOL_PROCESS_TIMEOUT_MS, [ref]$parsedTimeout) -and
                $parsedTimeout -ge 1000 -and $parsedTimeout -le 300000) {
                $configuredTimeoutMs = $parsedTimeout
            }
        }
        if ($configuredTimeoutMs -gt 0) {
            $deadline = (Get-Date).AddMilliseconds($configuredTimeoutMs)
        }
    }

    # Serial DFU idle watchdog: ignore DEBUG/INFO log prefixes when refreshing the idle clock,
    # otherwise verbose backends spam lines every few hundred ms and the host spinner stays at ~90% forever.
    # Over-broad regex "meaningful line" detection made this worse (it matched almost every line).
    # Opt-in legacy: NIUS_ADAFRUIT_DFU_IDLE_RESET_ON_ANY_LINE=1 treats DEBUG/INFO like any other line.
    $idleResetOnAnyLine = ($env:NIUS_ADAFRUIT_DFU_IDLE_RESET_ON_ANY_LINE -eq '1')
    $progressPulseInterval = Get-ProgressPulseIntervalTicks -FailureKind $FailureKind -Percent $ProgressPercent

    $output = ''

    if ($FailureKind -eq 'adafruit-dfu' -and $idleMs -gt 0) {
        $streamSync = [hashtable]::Synchronized(@{
                Lines = [System.Collections.ArrayList]::Synchronized((New-Object System.Collections.ArrayList))
                Out = New-Object System.Text.StringBuilder
                LastUtc = [datetime]::UtcNow
                IdleResetOnAnyLine = [bool]$idleResetOnAnyLine
                Hashes = 0
            })

        $idleSrcErr = 'nius-adf-err-' + ([Guid]::NewGuid().ToString('n'))

        # stderr stays line-based: adafruit-nrfutil's DEBUG/INFO verbose logs go
        # here and feed the idle watchdog.
        $null = Register-ObjectEvent -InputObject $process -EventName ErrorDataReceived -SourceIdentifier $idleSrcErr -MessageData $streamSync -Action {
            $ea = $EventArgs
            if ($null -ne $ea.Data -and $ea.Data.Length -gt 0) {
                $sync = $Event.MessageData
                [void]$sync.Lines.Add($ea.Data)
                $trimmed = $ea.Data.TrimStart()
                if ($sync.IdleResetOnAnyLine -or ($trimmed -notmatch '^(?i)(DEBUG|INFO)\s')) {
                    $sync.LastUtc = [datetime]::UtcNow
                }
            }
        }

        # stdout is read RAW on a background runspace rather than line-by-line.
        # adafruit-nrfutil prints one '#' per 512 B DFU frame but only emits a
        # newline every 40 frames, so a line reader sees progress just once per
        # ~20 KB (~4-5 updates total). Reading raw bytes lets us count every
        # frame and render a smooth 0-10-...-100 bar. Each '#' also refreshes
        # the idle clock, and all bytes are captured for failure-text scanning.
        $stdoutReaderScript = {
            param($proc, $sync)
            try {
                $sr = $proc.StandardOutput
                $buf = New-Object char[] 512
                while ($true) {
                    $n = $sr.Read($buf, 0, $buf.Length)
                    if ($n -le 0) { break }
                    $h = 0
                    for ($i = 0; $i -lt $n; $i++) {
                        $c = $buf[$i]
                        [void]$sync.Out.Append($c)
                        if ($c -eq [char]0x23) { $h++ }
                    }
                    if ($h -gt 0) { $sync.Hashes = [int]$sync.Hashes + $h }
                    $sync.LastUtc = [datetime]::UtcNow
                }
            }
            catch {
            }
        }
        $stdoutRunspace = [runspacefactory]::CreateRunspace()
        $stdoutRunspace.Open()
        $stdoutReader = [powershell]::Create()
        $stdoutReader.Runspace = $stdoutRunspace
        $null = $stdoutReader.AddScript($stdoutReaderScript).AddArgument($process).AddArgument($streamSync)
        $stdoutAsync = $stdoutReader.BeginInvoke()

        $process.BeginErrorReadLine()

        try {
            while ($true) {
                if ($process.WaitForExit(120)) {
                    break
                }

                $idleElapsed = ([datetime]::UtcNow - $streamSync.LastUtc).TotalMilliseconds
                if ($idleElapsed -gt $idleMs) {
                    $timedOutIdle = $true
                    try {
                        if (-not $process.HasExited) {
                            Stop-NiusProcessTree -RootPid $process.Id
                        }
                    }
                    catch {
                    }
                    $null = $process.WaitForExit(8000)
                    break
                }

                if ($null -ne $deadline -and (Get-Date) -gt $deadline) {
                    $timedOut = $true
                    try {
                        if (-not $process.HasExited) {
                            Stop-NiusProcessTree -RootPid $process.Id
                        }
                    }
                    catch {
                    }
                    $null = $process.WaitForExit(8000)
                    break
                }

                if ($byteProgress) {
                    # Real byte-based bar: map streamed frames onto 0..100% and
                    # emit a line each time we cross a new 10% band, so the IDE
                    # console shows the 0-10-20-...-100 steps (~11 lines).
                    $done = [int]$streamSync.Hashes
                    $frac = if ($TotalFrames -gt 0) { $done / [double]$TotalFrames } else { 0 }
                    if ($frac -lt 0) { $frac = 0 }
                    if ($frac -gt 1) { $frac = 1 }
                    $pct = [int][Math]::Floor($frac * 100)
                    if ($pct -gt 98) { $pct = 98 }
                    $band = [int][Math]::Floor($pct / 10)
                    if ($band -gt $lastShownBand) {
                        $lastShownBand = $band
                        $sentKb = [Math]::Round(($frac * $FirmwareBytes) / 1024.0, 1)
                        $detail = if ($fwKb -gt 0) { '{0}/{1} KB' -f $sentKb, $fwKb } else { '' }
                        Write-Stage -Percent $pct -Label $ProgressLabel -Detail $detail
                    }
                }
                elseif (($tick % $progressPulseInterval) -eq 0) {
                    Write-ProgressPulse -Percent $ProgressPercent -Label $ProgressLabel -Tick $tick
                }
                if ($FailureKind -eq 'adafruit-dfu' -and $tick -gt 0 -and ($tick % 80 -eq 0)) {
                    $hint = if ($streamSync.IdleResetOnAnyLine) {
                        'any nrfutil line resetting idle (NIUS_ADAFRUIT_DFU_IDLE_RESET_ON_ANY_LINE=1).'
                    }
                    else {
                        'non-DEBUG/INFO nrfutil lines resetting idle (DEBUG/INFO verbosity alone does not reset it).'
                    }
                    Write-NiusDetail ('[nius] DFU payload phase: adafruit-nrfutil may go quiet during transfer; watchdog fails after {0} ms without {1}' -f $idleMs, $hint) -ForegroundColor DarkGray
                }
                $tick += 1
            }
        }
        finally {
            foreach ($sub in @(Get-EventSubscriber | Where-Object { $_.SourceObject -eq $process })) {
                try {
                    Unregister-Event -SubscriptionId $sub.SubscriptionId -ErrorAction SilentlyContinue
                }
                catch {
                }
            }
            # Drain/stop the raw stdout reader runspace (it ends on stdout EOF,
            # which follows process exit; cap the wait so a stuck pipe can't hang).
            try {
                if ($null -ne $stdoutReader) {
                    if (-not $stdoutAsync.AsyncWaitHandle.WaitOne(2000)) {
                        $stdoutReader.Stop()
                    }
                    $stdoutReader.Dispose()
                }
            }
            catch {
            }
            try {
                if ($null -ne $stdoutRunspace) {
                    $stdoutRunspace.Close()
                    $stdoutRunspace.Dispose()
                }
            }
            catch {
            }
            Start-Sleep -Milliseconds 120
        }

        if (-not $timedOutIdle) {
            $null = $process.WaitForExit(0)
        }

        $stdoutText = ''
        try { $stdoutText = $streamSync.Out.ToString() } catch { }
        $output = (($stdoutText, (@($streamSync.Lines) -join [Environment]::NewLine)) -join [Environment]::NewLine).Trim()
    }
    else {
        # Drain both redirected pipes while the tool runs. Waiting for process
        # exit before ReadToEnd can deadlock when verbose OpenOCD, J-Link, or
        # package generation fills an OS pipe buffer.
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        while ($true) {
            if ($process.WaitForExit(120)) {
                break
            }
            if ($null -ne $deadline -and (Get-Date) -gt $deadline) {
                $timedOut = $true
                try {
                    if (-not $process.HasExited) {
                        Stop-NiusProcessTree -RootPid $process.Id
                    }
                }
                catch {
                }
                $null = $process.WaitForExit(8000)
                break
            }
            if (($tick % $progressPulseInterval) -eq 0) {
                Write-ProgressPulse -Percent $ProgressPercent -Label $ProgressLabel -Tick $tick
            }
            $tick += 1
        }

        if ($timedOut) {
            # Output-drain faults must not replace the primary process timeout.
            $stdout = ''
            $stderr = ''
            try {
                if ($stdoutTask.Wait(2000)) { $stdout = $stdoutTask.GetAwaiter().GetResult() }
            }
            catch { }
            try {
                if ($stderrTask.Wait(2000)) { $stderr = $stderrTask.GetAwaiter().GetResult() }
            }
            catch { }
        }
        else {
            $stdout = $stdoutTask.GetAwaiter().GetResult()
            $stderr = $stderrTask.GetAwaiter().GetResult()
        }
        $output = (($stdout, $stderr) -join [Environment]::NewLine).Trim()
    }

    if ($timedOutIdle) {
        $idleKind = if ($idleResetOnAnyLine) {
            'adafruit-nrfutil stdout/stderr lines'
        }
        else {
            'non-DEBUG/INFO adafruit-nrfutil output lines'
        }
        $summaryOutput = if ([string]::IsNullOrWhiteSpace($output)) {
            ('adafruit-nrfutil stalled: no new {0} for {1} ms (upload failed). Tune NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS; NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS=0 disables idle watchdog. NIUS_ADAFRUIT_DFU_IDLE_RESET_ON_ANY_LINE=1 makes DEBUG/INFO lines reset the idle clock (legacy; can spin forever if logs spam).' -f $idleKind, $idleMs)
        }
        else {
            ('adafruit-nrfutil stalled: no new {0} for {1} ms (upload failed). Tune NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS; NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS=0 disables idle watchdog. NIUS_ADAFRUIT_DFU_IDLE_RESET_ON_ANY_LINE=1 restores DEBUG/INFO-sensitive idle reset. Partial output:{2}{3}' -f $idleKind, $idleMs, [Environment]::NewLine, $output)
        }
        Throw-NiusUploadFailure (New-UploadFailure -Kind 'adafruit-dfu' -ExitCode 125 -Output $summaryOutput -Exe $Exe)
    }

    if ($timedOut) {
        $label = switch ($FailureKind) {
            'adafruit-genpkg' { 'adafruit-nrfutil genpkg' }
            'adafruit-dfu' { 'adafruit-nrfutil serial DFU' }
            'jlink' { 'SEGGER J-Link' }
            'openocd' { 'OpenOCD' }
            'uf2-convert' { 'UF2 converter' }
            'image-preflight' { 'application image preflight' }
            default { $FailureKind }
        }
        $summaryOutput = if ([string]::IsNullOrWhiteSpace($output)) {
            ('{0} exceeded process timeout ({1} ms); killed to release COM port.' -f $label, $configuredTimeoutMs)
        }
        else {
            ('{0} exceeded process timeout ({1} ms); killed to release COM port. Tool output:{2}{3}' -f $label, $configuredTimeoutMs, [Environment]::NewLine, $output)
        }
        Throw-NiusUploadFailure (New-UploadFailure -Kind $FailureKind -ExitCode 124 -Output $summaryOutput -Exe $Exe)
    }

    $textSignaledFailure = $false
    if ($FailureKind -eq 'adafruit-dfu' -and -not [string]::IsNullOrWhiteSpace($output)) {
        $textSignaledFailure = ($output -match '(?ims)(^Failed to upgrade target\.|Traceback\s*\(|NordicSemiException|No data received on serial port|Not able to proceed|\berror:\s*)')
    }
    elseif ($FailureKind -eq 'jlink' -and -not [string]::IsNullOrWhiteSpace($output)) {
        # SEGGER J-Link Commander can exit 0 after script-level target failures.
        # Treat fatal attach/programming text as a failed upload so a bootloader
        # burn never reports success when SWD never reached the nRF52.
        $textSignaledFailure = ($output -match '(?ims)(Could not connect to the target device|Failed to attach to CPU|Cannot connect to target|Error while identifying target|No device found on SWD|Target connection not established)')
    }

    if ($process.ExitCode -ne 0 -or $textSignaledFailure) {
        $reportedExitCode = if ($process.ExitCode -ne 0) { $process.ExitCode } else { 1 }
        Throw-NiusUploadFailure (New-UploadFailure -Kind $FailureKind -ExitCode $reportedExitCode -Output $output -Exe $Exe)
    }
}

function Invoke-JLinkDeploy {
    param(
        [string]$JLinkExe,
        [string]$HexPath,
        [string]$Device = ''
    )

    Assert-InputArtifact -Path $HexPath -Label 'hex'
    if ([string]::IsNullOrWhiteSpace($Device)) {
        $Device = 'NRF52840_XXAA'
    }

    $resolvedHex = (Resolve-Path -LiteralPath $HexPath).Path
    if ($resolvedHex -match '[\x00-\x1F\x7F"]') {
        throw 'J-Link firmware paths must not contain quotes or control characters.'
    }
    $scriptPath = Join-Path ([System.IO.Path]::GetTempPath()) ('nius-jlink-{0}.jlink' -f ([Guid]::NewGuid().ToString('n')))
    @(
        'r',
        'h',
        ('loadfile "{0}"' -f $resolvedHex),
        'r',
        'g',
        'q'
    ) | Set-Content -LiteralPath $scriptPath -Encoding ASCII

    try {
        Write-Stage -Percent 0 -Label 'Connecting' -Detail 'SEGGER J-Link SWD'
        Write-NiusDetail ('[nius] Resolved SEGGER J-Link: {0}' -f $JLinkExe) -ForegroundColor DarkGray
        Write-NiusDetail ('[nius] J-Link device: {0}' -f $Device) -ForegroundColor DarkGray
        $jlinkArguments = New-NiusJLinkCommandArguments -Device $Device -CommandFile $scriptPath
        Invoke-CommandChecked -Exe $JLinkExe -Arguments $jlinkArguments -FailureKind 'jlink' -ProgressPercent 76 -ProgressLabel 'SEGGER J-Link flash transaction active'
        Write-NiusUploadComplete -Note 'SEGGER J-Link SWD upload path'
    }
    finally {
        Remove-Item -LiteralPath $scriptPath -Force -ErrorAction SilentlyContinue
    }
}

function Assert-NiusBootloaderHexApprotectPolicy {
    param([string]$HexPath)

    # nRF52 UICR.APPROTECT is at 0x10001208. For the nRF52840 boards supported
    # here, an erased low byte (0xFF) means debug access remains open. Values
    # such as 0x00 enable APPROTECT; 0x5A is not the nRF52840 disable value and
    # has been observed to close SWD access on board recovery. Leaving the word
    # unmentioned is also acceptable because it preserves the target's current
    # debug policy.
    $targetAddress = 0x10001208
    $base = 0
    $seenApprotect = $false
    $approtectByte = $null

    foreach ($line in [System.IO.File]::ReadLines($HexPath)) {
        $trimmed = $line.Trim()
        if ($trimmed.Length -lt 11 -or -not $trimmed.StartsWith(':')) { continue }
        try {
            $count = [Convert]::ToInt32($trimmed.Substring(1, 2), 16)
            $offset = [Convert]::ToInt32($trimmed.Substring(3, 4), 16)
            $recordType = [Convert]::ToInt32($trimmed.Substring(7, 2), 16)
            if ($recordType -eq 4 -and $count -eq 2) {
                $base = ([Convert]::ToInt32($trimmed.Substring(9, 4), 16) -shl 16)
                continue
            }
            if ($recordType -ne 0 -or $count -le 0) { continue }
            $absolute = $base + $offset
            if ($targetAddress -lt $absolute -or $targetAddress -ge ($absolute + $count)) { continue }
            $byteIndex = $targetAddress - $absolute
            $approtectByte = [Convert]::ToInt32($trimmed.Substring(9 + ($byteIndex * 2), 2), 16)
            $seenApprotect = $true
            break
        }
        catch {
            continue
        }
    }

    if ($seenApprotect -and $approtectByte -ne 0xFF) {
        Throw-NiusUploadFailure (New-UploadFailure -Kind 'jlink' -ExitCode 1 -Output (@(
                    ('Refusing to flash bootloader HEX: it writes nRF52 UICR.APPROTECT (0x10001208) low byte 0x{0:X2}.' -f $approtectByte),
                    'For this nRF52 bootloader path the image must leave APPROTECT erased/unprogrammed, or write 0xFF, so J-Link recovery remains possible.',
                    'ZH: Bootloader HEX writes a non-erased APPROTECT value; blocked to keep J-Link recovery possible.'
                ) -join [Environment]::NewLine) -Exe $toolPath)
    }

    if ($seenApprotect -and $approtectByte -eq 0xFF) {
        Write-NiusDetail '[nius] Bootloader HEX leaves nRF52 APPROTECT disabled/erased (UICR.APPROTECT=0xFF).' -ForegroundColor DarkGray
    }
    elseif (-not $seenApprotect) {
        Write-NiusDetail '[nius] Bootloader HEX does not program nRF52 APPROTECT; existing target/debug-access policy is not changed by the image.' -ForegroundColor DarkGray
    }
}

function Assert-NiusBootloaderImage {
    param(
        [string]$HexPath,
        [string]$TargetFlashEnd,
        [string]$TargetRamEnd
    )

    if ([string]::IsNullOrWhiteSpace($TargetFlashEnd) -or
        [string]::IsNullOrWhiteSpace($TargetRamEnd)) {
        throw 'Bootloader recovery requires exact target flash and SRAM ceilings.'
    }
    $python = Resolve-PythonLaunch
    $validator = Join-Path $PSScriptRoot 'build_uf2.py'
    Assert-ToolExists -Path $validator
    $arguments = @()
    $arguments += $python.PrefixArgs
    $arguments += @(
        $validator,
        '--input-hex', $HexPath,
        '--family-id', '0',
        '--validate-only',
        '--bootloader-image',
        '--flash-end', $TargetFlashEnd,
        '--ram-end', $TargetRamEnd
    )
    Invoke-CommandChecked -Exe $python.Exe -Arguments $arguments -FailureKind 'image-preflight'
}

function Invoke-NiusBootloaderDeploy {
    param(
        [string]$OpenOcdExe,
        [string]$ScriptRootPath,
        [string]$OpenOcdConfig,
        [string]$BootloaderHexPath,
        [string]$Protocol,
        [string]$Device = '',
        [string]$TargetFlashEnd = '',
        [string]$TargetRamEnd = ''
    )

    Assert-InputArtifact -Path $BootloaderHexPath -Label 'bootloader hex'
    # This runs before nrf52_recover/erase. A damaged, cross-capacity, missing-
    # vector, or debug-locking recovery image must never erase a working target.
    Assert-NiusBootloaderImage `
        -HexPath $BootloaderHexPath `
        -TargetFlashEnd $TargetFlashEnd `
        -TargetRamEnd $TargetRamEnd
    Assert-NiusBootloaderHexApprotectPolicy -HexPath $BootloaderHexPath

    $isJLink = ($Protocol -match '(?i)jlink')
    if ($isJLink) {
        $jlinkExe = Resolve-NiusJLinkExe
        if ([string]::IsNullOrWhiteSpace($Device)) {
            $Device = 'NRF52840_XXAA'
        }
        $resolvedBootloaderHex = (Resolve-Path -LiteralPath $BootloaderHexPath).Path
        if ($resolvedBootloaderHex -match '[\x00-\x1F\x7F"]') {
            throw 'J-Link bootloader paths must not contain quotes or control characters.'
        }
        $scriptPath = Join-Path ([System.IO.Path]::GetTempPath()) ('nius-jlink-bootloader-{0}.jlink' -f ([Guid]::NewGuid().ToString('n')))
        @(
            'r',
            'h',
            'erase',
            ('loadfile "{0}"' -f $resolvedBootloaderHex),
            'r',
            'g',
            'q'
        ) | Set-Content -LiteralPath $scriptPath -Encoding ASCII

        try {
            Write-Stage -Percent 0 -Label 'Connecting' -Detail 'SEGGER J-Link bootloader flash'
            Write-NiusDetail ('[nius] Resolved SEGGER J-Link: {0}' -f $jlinkExe) -ForegroundColor DarkGray
            $jlinkArguments = New-NiusJLinkCommandArguments -Device $Device -CommandFile $scriptPath
            Invoke-CommandChecked -Exe $jlinkExe -Arguments $jlinkArguments -FailureKind 'jlink' -ProgressPercent 76 -ProgressLabel 'SEGGER J-Link bootloader flash active'
            Write-NiusUploadComplete -Note 'SEGGER J-Link bootloader flash path'
        }
        finally {
            Remove-Item -LiteralPath $scriptPath -Force -ErrorAction SilentlyContinue
        }
        return
    }

    Assert-ToolExists -Path $OpenOcdExe
    if ([string]::IsNullOrWhiteSpace($ScriptRootPath) -or [string]::IsNullOrWhiteSpace($OpenOcdConfig)) {
        Throw-NiusUploadFailure (New-UploadFailure -Kind 'openocd' -ExitCode 1 -Output 'OpenOCD script root/config missing from bootloader recipe.' -Exe $OpenOcdExe)
    }

    $resolvedBootloaderHex = (Resolve-Path -LiteralPath $BootloaderHexPath).Path
    $openOcdCommand = 'telnet_port disabled; init; halt; nrf52_recover; reset halt; program {0} verify reset; shutdown' -f `
        (ConvertTo-OpenOcdTclWord -Value $resolvedBootloaderHex)
    $openOcdArguments = New-NiusOpenOcdArguments `
        -ScriptRootPath $ScriptRootPath `
        -ConfigPath $OpenOcdConfig `
        -Command $openOcdCommand
    Write-Stage -Percent 0 -Label 'Connecting' -Detail 'OpenOCD bootloader flash'
    Invoke-CommandChecked -Exe $OpenOcdExe -Arguments $openOcdArguments -FailureKind 'openocd' -ProgressPercent 76 -ProgressLabel 'OpenOCD bootloader flash active'
    Write-NiusUploadComplete -Note 'OpenOCD bootloader flash path'
}

function Get-RelevantLogLines {
    param(
        [string]$Output,
        [int]$TailCount = 5
    )

    if ([string]::IsNullOrWhiteSpace($Output)) {
        return @()
    }

    [array]$lines = @($Output -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($lines.Count -le $TailCount) {
        return $lines
    }

    return @($lines | Select-Object -Last $TailCount)
}

function Get-FailureHints {
    param(
        [string]$Kind,
        [string]$Output
    )

    $normalized = if ([string]::IsNullOrWhiteSpace($Output)) { '' } else { $Output.ToLowerInvariant() }

    switch ($Kind) {
        'uf2-wait' {
            if ($normalized -match 'problem_code=10|cm_prob_failed_start|usb mass storage') {
                return @(
                    'Windows sees the target UF2 mass-storage interface, but it did not become ready, so no matching drive letter can be assigned.',
                    'ArduinoNRF did not reset a USB hub or modify any Windows driver. Leave peer devices untouched and retry only after the selected board is visible again.',
                    'Use distinct runtime and bootloader USB identities so Windows does not reuse one interface cache for incompatible CDC and MSC functions.'
                )
            }
            return @(
                'The selected board never exposed a matching UF2 volume for this upload mode.',
                'If this board family only supports serial DFU on this bootloader, switch to the matching serial-DFU menu entry.',
                'If the hardware should expose UF2 MSC, recover or replace the bootloader via SWD and retest.'
            )
        }
        'dfu-wait' {
            return @(
                'If the board is a clone, verify whether it actually enters UF2 instead of Nordic DFU.',
                'Try the Bootloader mode menu that matches the detected bootloader family.',
                'If no USB bootloader appears, use the SWD upload mode to recover the board.'
            )
        }
        'dfu' {
            return @(
                'Re-enter bootloader and keep the USB cable stable during transfer.',
                'Check that the selected board and bootloader mode match the real device identity.',
                'If this repeats, switch to SWD upload to separate firmware issues from bootloader issues.'
            )
        }
        'openocd' {
            return @(
                'Confirm SWDIO, SWCLK, GND, and target power are all present.',
                'If the probe is seen but programming fails, reduce cable length or retry after a full power cycle.',
                'The firmware already compiles; this class of failure usually means wiring, probe, or target state.'
            )
        }
        'jlink' {
            return @(
                'Confirm SWDIO, SWCLK, GND, and target power are all present.',
                'Install SEGGER J-Link Software, or set NIUS_JLINK_PATH to JLink.exe / the J-Link installation directory.',
                'Use Upload Method -> SWD programmer (SEGGER J-Link) only for SEGGER probes; CMSIS-DAP probes should use the CMSIS-DAP SWD option.'
            )
        }
        'uf2-convert' {
            return @(
                'Check that Python 3 is installed and reachable as py -3 or python.',
                'A missing .hex file usually means the compile step did not complete successfully.',
                'If the converter runs but deploy still fails, verify the UF2 volume is mounted and writable.'
            )
        }
        'adafruit-dfu' {
            if ($normalized -match 'wrong com for adafruit serial dfu|user cdc serial cannot') {
                return @(
                    'Arduino IDE: Tools > Port -> pick the SERVICE / maintenance CDC COM (usually MI_00, smaller index).',
                    'ZH: Use maintenance COM (Service CDC / MI_00), not the Serial.print USER CDC.',
                    'Automatic same-device remap is normally enabled; NIUS_REJECT_USER_CDC_UPLOAD_PORT=1 makes this diagnostic strict.'
                )
            }
            if ($normalized -match 'stalled: no new|stalled: no stdout/stderr lines') {
                return @(
                    'nrfutil idle watchdog: no non-DEBUG/INFO lines for NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS (default 30000 ms) means the transfer is treated as stuck.',
                    'Increase the timeout on slow USB links; NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS=0 disables idle detection (process wall timeout still applies).',
                    'NIUS_ADAFRUIT_DFU_IDLE_RESET_ON_ANY_LINE=1 restores counting DEBUG/INFO lines toward idle reset (can spin forever if logs spam).',
                    'Otherwise: bootloader not responding, wrong COM, broken adafruit-nrfutil/Python env (reinstall with `pip install -U adafruit-nrfutil`), or another program holding the serial port.'
                )
            }
            return @(
                'Make sure adafruit-nrfutil is installed and reachable: `pip install adafruit-nrfutil`.',
                'Confirm the selected COM port matches the bootloader''s CDC interface (Tools > Port).',
                'If the device is in user mode, enable 1200 bps touch in the upload recipe so adafruit-nrfutil can reset it into bootloader mode.',
                'If genpkg fails, double-check the .hex artifact path and that python is not in a broken venv.'
            )
        }
        'post-verify' {
            return @(
                'The transfer finished, but the board stayed in the UF2/serial-DFU bootloader instead of launching the flashed application.',
                'If this clone sometimes mounts a UF2 drive letter (for example J:), use the explicit UF2 bootloader menu entry or let the wrapper''s UF2 fallback retry path handle it.',
                'On clone boards this usually means the bootloader never marked the app as valid; switch to SWD upload/recovery to separate firmware issues from bootloader/settings corruption.',
                'If the board has no button, USB-only recovery is limited once the bootloader keeps re-entering itself on every power cycle.'
            )
        }
        'layout' {
            return @(
                'Arduino compiles the sketch before upload; upload.ps1 cannot relocate an already-linked image.',
                'Re-select the Bootloader / DFU option whose app start matches the mounted bootloader, then compile and upload again.',
                'For a UF2 drive that reports `SoftDevice: not found`, use a no-SoftDevice / MBR only (0x1000) option.',
                'The board was left in UF2/DFU; no firmware was written when this guard fired before transfer.',
                'Serial DFU now requires a scoped UF2 volume with INFO_UF2.TXT on the selected board before transfer; double-tap RESET if the drive is missing.'
            )
        }
        'misflash' {
            return @(
                'The transfer completed, but USB did not re-enumerate in application mode - typical when app start in Tools does not match the bootloader flash layout.',
                'For ProMicro-class clones with `SoftDevice: not found`, use Bootloader / DFU -> no SoftDevice / MBR only (0x1000).',
                'Double-tap RESET or re-plug USB to enter UF2, fix the menu, recompile, upload again. Use SWD/J-Link if the port stays missing.',
                'ZH: USB serial missing after upload usually means wrong Bootloader / DFU app start; fix the menu, recompile, upload again.'
            )
        }
        default {
            if ($normalized -match 'required .* artifact not found|not have compiled correctly') {
                return @(
                    'The upload wrapper did not receive the compiled firmware artifact.',
                    'This usually means the build failed before upload or the build path is stale.',
                    'Rebuild once, then retry upload with the same board options.'
                )
            }

            if ($normalized -match 'upload tool not found') {
                return @(
                    'The Arduino tool package layout is incomplete or mismatched with platform.txt.',
                    'Reinstall the core tools package before retrying upload.'
                )
            }

            return @('Inspect the short trace below for the last useful tool message.')
        }
    }
}

function New-Uf2Artifact {
    param(
        [string]$HexPath,
        [string]$FamilyId,
        [string]$AppStart,
        [string]$MaxSize,
        [string]$RamEnd
    )

    if ([string]::IsNullOrWhiteSpace($HexPath) -or -not (Test-Path -LiteralPath $HexPath)) {
        throw ('Compiled hex file not found for UF2 upload: {0}' -f $HexPath)
    }

    if ([string]::IsNullOrWhiteSpace($FamilyId)) {
        throw 'UF2 upload was selected, but no UF2 family ID was provided by the board recipe.'
    }
    if ([string]::IsNullOrWhiteSpace($AppStart) -or
        [string]::IsNullOrWhiteSpace($MaxSize) -or
        [string]::IsNullOrWhiteSpace($RamEnd)) {
        throw 'UF2 upload was selected, but the board recipe did not provide a complete application memory contract.'
    }

    $python = Resolve-PythonLaunch
    $converter = Join-Path $PSScriptRoot 'build_uf2.py'
    Assert-ToolExists -Path $converter
    # Keep converter output invocation-owned. A failed conversion/copy must not
    # leave a stale sibling of the compiled HEX that a later upload can reuse.
    $uf2Path = Join-Path ([System.IO.Path]::GetTempPath()) `
        ('arduinonrf-app-{0}.uf2' -f ([Guid]::NewGuid().ToString('n')))
    $args = @()
    $args += $python.PrefixArgs
    $args += @(
        $converter,
        '--input-hex', $HexPath,
        '--output-uf2', $uf2Path,
        '--family-id', $FamilyId,
        '--app-start', $AppStart,
        '--max-size', $MaxSize,
        '--ram-end', $RamEnd
    )
    try {
        Invoke-CommandChecked -Exe $python.Exe -Arguments $args -FailureKind 'uf2-convert'
        return $uf2Path
    }
    catch {
        if ([System.IO.File]::Exists($uf2Path)) {
            try { [System.IO.File]::Delete($uf2Path) } catch { }
        }
        throw
    }
}

function Assert-NiusApplicationImage {
    param(
        [string]$HexPath,
        [string]$FamilyId,
        [string]$AppStart,
        [string]$MaxSize,
        [string]$RamEnd
    )

    Assert-InputArtifact -Path $HexPath -Label 'hex'
    if ([string]::IsNullOrWhiteSpace($FamilyId)) {
        $FamilyId = '0xADA52840'
    }
    if ([string]::IsNullOrWhiteSpace($AppStart) -or
        [string]::IsNullOrWhiteSpace($MaxSize) -or
        [string]::IsNullOrWhiteSpace($RamEnd)) {
        throw 'The board recipe did not provide a complete application memory contract.'
    }

    $python = Resolve-PythonLaunch
    $validator = Join-Path $PSScriptRoot 'build_uf2.py'
    $args = @()
    $args += $python.PrefixArgs
    $args += @(
        $validator,
        '--input-hex', $HexPath,
        '--family-id', $FamilyId,
        '--app-start', $AppStart,
        '--max-size', $MaxSize,
        '--ram-end', $RamEnd,
        '--validate-only'
    )
    Invoke-CommandChecked -Exe $python.Exe -Arguments $args -FailureKind 'image-preflight'
}

function New-UploadFailure {
    param(
        [string]$Kind,
        [int]$ExitCode,
        [string]$Output,
        [string]$Exe
    )

    return [pscustomobject]@{
        PSTypeName = 'NiusUploadFailure'
        Kind = $Kind
        ExitCode = $ExitCode
        Summary = Get-FailureSummary -Kind $Kind -ExitCode $ExitCode -Output $Output -Exe $Exe
        Hints = @(Get-FailureHints -Kind $Kind -Output $Output)
        Details = @(
            Get-RelevantLogLines -Output $Output -TailCount $(if ($Kind -eq 'adafruit-dfu' -or $Kind -eq 'adafruit-genpkg') { 18 } else { 5 })
        )
    }
}

function Throw-NiusUploadFailure {
    param(
        [Parameter(Mandatory)][object]$Failure
    )

    $ex = New-Object System.Management.Automation.RuntimeException($Failure.Summary)
    $record = New-Object System.Management.Automation.ErrorRecord($ex, 'NiusUploadFailure', [System.Management.Automation.ErrorCategory]::OperationStopped, $Failure)
    throw $record
}

function Get-NiusUploadFailureFromCatch {
    param([System.Management.Automation.ErrorRecord]$ErrorRecord)

    if ($null -eq $ErrorRecord) {
        return $null
    }

    $t = $ErrorRecord.TargetObject
    if ($t -and $t.PSTypeNames -contains 'NiusUploadFailure') {
        return $t
    }

    $rex = $ErrorRecord.Exception
    while ($null -ne $rex) {
        if ($rex -is [System.Management.Automation.RuntimeException] -and $rex.ErrorRecord -and $rex.ErrorRecord.TargetObject -and $rex.ErrorRecord.TargetObject.PSTypeNames -contains 'NiusUploadFailure') {
            return $rex.ErrorRecord.TargetObject
        }
        $rex = $rex.InnerException
    }

    return $null
}

function Get-FailureSummary {
    param(
        [string]$Kind,
        [int]$ExitCode,
        [string]$Output,
        [string]$Exe
    )

    $normalized = if ([string]::IsNullOrWhiteSpace($Output)) { '' } else { $Output.ToLowerInvariant() }
    $formattedExitCode = Format-ExitCode -Code $ExitCode

    if ($Kind -eq 'uf2-wait') {
        if (-not [string]::IsNullOrWhiteSpace($Output)) {
            return $Output
        }

        return ('Board did not present a matching UF2 volume after 1200 bps touch. Expected label "{0}", model "{1}", board-id "{2}".' -f $Uf2VolumeLabel, $Uf2Model, $Uf2BoardId)
    }

    if ($Kind -eq 'dfu-wait') {
        $uf2 = $null
        if (-not [string]::IsNullOrWhiteSpace($script:NiusUploadCompositeStableId)) {
            try {
                $uf2 = Get-Uf2ProbeSummary -PreferredCompositeStableId $script:NiusUploadCompositeStableId
            }
            catch {
                $uf2 = $null
            }
        }
        if ($uf2) {
            return ('Board entered UF2 bootloader instead of Nordic DFU: expected {0}:{1}, but detected UF2 volume {2} labeled "{3}" with model "{4}" and board-id "{5}".' -f $UsbVid, $UsbPid, $uf2.Drive, $uf2.Label, $uf2.Model, $uf2.BoardId)
        }

        return ('Board stayed in application mode: no DFU device {0}:{1} appeared after 1200 bps touch.' -f $UsbVid, $UsbPid)
    }

    if ($Kind -eq 'openocd') {
        if ($ExitCode -lt 0 -or $normalized -match 'access violation|exception') {
            return ('OpenOCD crashed before flashing completed. exit={0}; probe/tool path={1}' -f $formattedExitCode, $Exe)
        }

        if ($normalized -match 'unable to find cmsis-dap device|no device found|unable to open cmsis-dap|cmsis-dap command failed|could not find or open device') {
            return ('Probe not detected: OpenOCD could not talk to a CMSIS-DAP/J-Link-compatible debug probe. exit={0}' -f $formattedExitCode)
        }

        if ($normalized -match 'target not halted|timed out while waiting for target halted|could not halt target|init mode failed') {
            return ('Target not halted: the probe connected, but the nRF52 did not enter a programmable halted state. Check SWDIO/SWCLK/GND/Vref contact and power.' )
        }

        return ('OpenOCD failed while programming the target. exit={0}; inspect the log above for the last OpenOCD message.' -f $formattedExitCode)
    }

    if ($Kind -eq 'dfu') {
        if ($normalized -match 'no dfu capable usb device available|cannot open dfu device|error during download|get_status') {
            return ('USB DFU failed: board did not present a usable DFU endpoint or dropped out during transfer. exit={0}' -f $formattedExitCode)
        }

        return ('USB DFU command failed. exit={0}; inspect the log above for the last DFU tool message.' -f $formattedExitCode)
    }

    if ($Kind -eq 'uf2-convert') {
        return ('UF2 conversion failed before deployment. exit={0}; inspect the converter output above.' -f $formattedExitCode)
    }

    if ($Kind -eq 'adafruit-dfu') {
        if ($normalized -match 'stalled: no new|stalled: no stdout/stderr lines') {
            return ('Adafruit serial DFU stalled: nrfutil produced no non-DEBUG/INFO lines within NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS (upload failed). exit={0}; raise the timeout on slow links; NIUS_ADAFRUIT_DFU_IDLE_RESET_ON_ANY_LINE=1 changes idle semantics.' -f $formattedExitCode)
        }
        if ($normalized -match 'wrong com for adafruit serial dfu') {
            return ('Wrong COM: USER CDC cannot run Adafruit serial DFU - switch IDE Port to SERVICE CDC (MI_00). exit={0}' -f $formattedExitCode)
        }
        if ($normalized -match 'no module named|importerror') {
            return ('adafruit-nrfutil is installed but its Python environment is incomplete. exit={0}; reinstall the tool with `pip install --upgrade adafruit-nrfutil`.' -f $formattedExitCode)
        }
        if ($normalized -match 'could not open port|access is denied|permission denied') {
            return ('Adafruit serial DFU could not open the COM port. exit={0}; another process (PuTTY, Arduino IDE Serial Monitor, the IDE 2 debug bridge) may be holding it.' -f $formattedExitCode)
        }
        if ($normalized -match 'no response from device|timed out|timeout') {
            return ('Adafruit serial DFU timed out waiting for bootloader response on the selected COM port. exit={0}; verify the bootloader is running and the right port is selected.' -f $formattedExitCode)
        }
        return ('Adafruit serial DFU command failed. exit={0}; inspect the log above for the last adafruit-nrfutil message.' -f $formattedExitCode)
    }

    if ($Kind -eq 'post-verify') {
        return ('Firmware transfer completed, but the board never left the Adafruit UF2 bootloader. Snapshot: {0}' -f $Output)
    }

    if ($Kind -eq 'layout' -or $Kind -eq 'misflash') {
        if (-not [string]::IsNullOrWhiteSpace($Output)) {
            return (($Output -split "[\r\n]+" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -First 2) -join ' ')
        }
    }

    return ('Command failed with exit code {0}: {1}' -f $formattedExitCode, $Exe)
}

function Watch-SerialPortPostTouchResetCycle {
    param(
        [string]$PortName,
        [int]$TimeoutMs = 900
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        return $false
    }

    $normalizedPort = $PortName.Trim().ToUpperInvariant()
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    $sawDetach = $false
    while ((Get-Date) -lt $deadline) {
        $portPresent = (@([System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object { $_.Trim().ToUpperInvariant() }) -contains $normalizedPort)
        if (-not $portPresent) {
            $sawDetach = $true
        }
        elseif ($sawDetach) {
            return $true
        }
        Start-Sleep -Milliseconds 35
    }

    return $false
}

function Touch-SerialPort1200 {
    param(
        [string]$PortName,
        [bool]$ObservePostTouchResetCycle = $false
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        return [pscustomobject]@{
            Triggered = $false
            SawResetCycle = $false
            FailureKind = 'invalid-port'
            Error = 'serial port is not concrete'
        }
    }

    function Invoke-TouchPulse {
        param([int]$BaudRate)

        $openTimeoutSec = 3
        $envMs = $env:NIUS_TOUCH_SERIAL_OPEN_TIMEOUT_MS
        if (-not [string]::IsNullOrWhiteSpace($envMs)) {
            $parsedOpen = -1
            if ([int]::TryParse($envMs, [ref]$parsedOpen) -and
                $parsedOpen -ge 500 -and $parsedOpen -le 10000) {
                $openTimeoutSec = [int][Math]::Ceiling($parsedOpen / 1000.0)
                if ($openTimeoutSec -lt 1) {
                    $openTimeoutSec = 1
                }
            }
        }

        # SerialPort.Open()/Dispose() can wedge in the Windows serial stack when
        # the 1200-bps touch removes the device underneath the handle. Isolate
        # that handle in one hidden helper process. If the exact COM disappears
        # or the bounded timeout expires, only that helper PID is terminated;
        # the uploader never touches a USB hub, device node, or driver.
        $touchSb = {
            $ProgressPreference = 'SilentlyContinue'
            $ErrorActionPreference = 'Stop'
            $handle = [IntPtr]::Zero
            try {
                $pn = $env:NIUS_TOUCH_HELPER_PORT
                $br = [int]$env:NIUS_TOUCH_HELPER_BAUD

                # System.IO.Ports.SerialPort.Close()/Dispose() can remain in an
                # uninterruptible usbser.sys wait after the touch disconnects a
                # native-USB board. Use the Win32 serial API directly so the
                # short-lived helper owns exactly one kernel handle and can be
                # terminated without leaving a managed finalizer blocked on it.
                Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class NiusNativeSerialTouch {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr CreateFile(
        string name, uint access, uint share, IntPtr security,
        uint creation, uint flags, IntPtr templateFile);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool DeviceIoControl(
        IntPtr handle, uint code, ref uint input, uint inputSize,
        IntPtr output, uint outputSize, out uint returned, IntPtr overlapped);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool DeviceIoControl(
        IntPtr handle, uint code, IntPtr input, uint inputSize,
        IntPtr output, uint outputSize, out uint returned, IntPtr overlapped);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool CloseHandle(IntPtr handle);
}
'@
                $invalidHandle = [IntPtr]::new(-1)
                $handle = [NiusNativeSerialTouch]::CreateFile(
                    ('\\.\' + $pn),
                    [uint32]3221225472,
                    0,
                    [IntPtr]::Zero,
                    3,
                    0,
                    [IntPtr]::Zero)
                if ($handle -eq $invalidHandle) {
                    throw [ComponentModel.Win32Exception]::new(
                        [Runtime.InteropServices.Marshal]::GetLastWin32Error(),
                        ('cannot open ' + $pn))
                }
                $returned = [uint32]0
                $nativeBaud = [uint32]$br
                if (-not [NiusNativeSerialTouch]::DeviceIoControl(
                        $handle, 0x001B0004, [ref]$nativeBaud, 4,
                        [IntPtr]::Zero, 0, [ref]$returned, [IntPtr]::Zero)) {
                    throw [ComponentModel.Win32Exception]::new(
                        [Runtime.InteropServices.Marshal]::GetLastWin32Error(),
                        ('cannot set baud on ' + $pn))
                }
                $null = [NiusNativeSerialTouch]::DeviceIoControl(
                    $handle, 0x001B0024, [IntPtr]::Zero, 0,
                    [IntPtr]::Zero, 0, [ref]$returned, [IntPtr]::Zero) # SET_DTR
                $null = [NiusNativeSerialTouch]::DeviceIoControl(
                    $handle, 0x001B0030, [IntPtr]::Zero, 0,
                    [IntPtr]::Zero, 0, [ref]$returned, [IntPtr]::Zero) # SET_RTS
                Start-Sleep -Milliseconds 120
                # Core triggers bootloader prep when SERVICE CDC line coding is 1200 *and* host drops DTR
                # (CDC_REQ_SET_CONTROL_LINE_STATE: previousDtr && !dtr). Dispose alone can miss the edge on some hosts.
                if ($br -eq 1200) {
                    Start-Sleep -Milliseconds 80
                    if (-not [NiusNativeSerialTouch]::DeviceIoControl(
                            $handle, 0x001B0028, [IntPtr]::Zero, 0,
                            [IntPtr]::Zero, 0, [ref]$returned, [IntPtr]::Zero)) { # CLR_DTR
                        throw [ComponentModel.Win32Exception]::new(
                            [Runtime.InteropServices.Marshal]::GetLastWin32Error(),
                            ('cannot clear DTR on ' + $pn))
                    }
                    Start-Sleep -Milliseconds 100
                }
                exit 0
            }
            catch {
                try {
                    if ($_.Exception -is [ComponentModel.Win32Exception]) {
                        [Console]::Error.WriteLine(
                            ('WIN32:{0}:{1}' -f $_.Exception.NativeErrorCode, $_.Exception.Message))
                    }
                    else {
                        [Console]::Error.WriteLine($_.Exception.Message)
                    }
                } catch { }
                exit 1
            }
            finally {
                if ($handle -ne [IntPtr]::Zero -and $handle -ne [IntPtr]::new(-1)) {
                    try { $null = [NiusNativeSerialTouch]::CloseHandle($handle) } catch { }
                }
            }
        }

        $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($touchSb.ToString()))
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = (Join-Path $PSHOME 'powershell.exe')
        $psi.Arguments = ('-NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand {0}' -f $encoded)
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
        $psi.RedirectStandardError = $true
        $psi.EnvironmentVariables['NIUS_TOUCH_HELPER_PORT'] = $PortName
        $psi.EnvironmentVariables['NIUS_TOUCH_HELPER_BAUD'] = [string]$BaudRate
        $helper = [System.Diagnostics.Process]::Start($psi)
        $sawDetach = $false
        $openDeadline = [datetime]::UtcNow.AddSeconds($openTimeoutSec)
        while ([datetime]::UtcNow -lt $openDeadline) {
            if ($helper.WaitForExit(25)) {
                break
            }
            # A successful 1200/DTR pulse can reboot the board before Dispose
            # returns. Windows then blocks the worker in serial cleanup even
            # though the requested transition already happened. Treat the
            # selected port's real detach as success immediately instead of
            # paying the full Open timeout and retrying a board that is already
            # in its bootloader.
            if (-not (Test-SerialPortPnpPresent -PortName $PortName)) {
                $sawDetach = $true
                break
            }
        }
        if ($sawDetach) {
            if (-not $helper.HasExited) {
                try { $helper.Kill() } catch { }
                try { $null = $helper.WaitForExit(700) } catch { }
            }
            try { $helper.Dispose() } catch { }
            return [pscustomobject]@{
                Triggered = $true
                SawResetCycle = $true
                FailureKind = ''
                Error = ''
            }
        }
        if (-not $helper.HasExited) {
            try { $helper.Kill() } catch { }
            try { $null = $helper.WaitForExit(700) } catch { }
            try { $helper.Dispose() } catch { }
            return [pscustomobject]@{
                Triggered = $false
                SawResetCycle = $false
                FailureKind = 'timeout'
                Error = 'touch helper timed out while the selected COM remained present'
            }
        }

        $recv = ($helper.ExitCode -eq 0)
        $helperError = ''
        if (-not $recv) {
            try {
                $helperError = $helper.StandardError.ReadToEnd().Trim()
                if (-not [string]::IsNullOrWhiteSpace($helperError)) {
                    Write-NiusDetail ('[nius]   touch helper could not open {0}: {1}' -f $PortName, $helperError) -ForegroundColor DarkGray
                }
            }
            catch {
            }
        }
        try { $helper.Dispose() } catch { }

        return [pscustomobject]@{
            Triggered = [bool]$recv
            SawResetCycle = $false
            FailureKind = $(if (-not $recv -and $helperError -match '(?i)WIN32:5:') { 'busy' } elseif (-not $recv) { 'open-failed' } else { '' })
            Error = $helperError
        }
    }

    Write-NiusDetail ('[nius] One 1200bps touch attempt on {0} (bounded open timeout)...' -f $PortName) -ForegroundColor DarkGray
    $touchPulse = Invoke-TouchPulse -BaudRate 1200
    if (-not $touchPulse.Triggered) {
        Write-NiusDetail ('[warn] 1200bps touch failed on {0}: {1}' -f $PortName, $touchPulse.Error)
        return $touchPulse
    }
    if ($ObservePostTouchResetCycle) {
        $touchPulse.SawResetCycle = Watch-SerialPortPostTouchResetCycle -PortName $PortName
    }
    Start-Sleep -Milliseconds 200
    return $touchPulse
}

function Invoke-Touch1200Transition {
    param(
        [string]$PortName,
        [string]$BootloaderVid = '',
        [string]$BootloaderPid = '',
        [string]$RuntimeVid = '',
        [string]$RuntimePid = '',
        [string]$InterfaceParentPrefix = '',
        [string]$PreferredCompositeStableId = '',
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = ''
    )

    # A single 1200-baud open followed by a DTR falling edge is the maintained
    # ArduinoNRF bootloader-entry contract. Never issue a second inferred pulse
    # when host evidence is delayed: the first request may already be executing,
    # and a new COM with the same name may belong to the bootloader session.
    $touchMode = [pscustomobject]@{ Label = 'single 1200 touch' }

    # Per-mode transition-detect timeout. A successful touch normally produces
    # scoped bootloader evidence quickly.
    # Keep this bounded so a failed attempt cannot strand an unprobed board in a
    # host-side wait; override via NIUS_TOUCH_TRANSITION_TIMEOUT_MS when needed.
    $perModeTimeoutMs = 2500
    $o = $env:NIUS_TOUCH_TRANSITION_TIMEOUT_MS
    if (-not [string]::IsNullOrWhiteSpace($o)) {
        $tt = -1
        if ([int]::TryParse($o, [ref]$tt) -and $tt -ge 800 -and $tt -le 15000) { $perModeTimeoutMs = $tt }
    }

    # The cached serial inventory was populated by pre-touch port resolution on
    # the stable runtime port set; drop it now so anything that re-queries after
    # the board resets/re-enumerates sees fresh data.
    Clear-SerialPortInventoryCache
    # Same for the PnP-device snapshot: disarm + drop so the post-touch
    # transition pollers (which filter PnP every iteration) see fresh data.
    Set-PnpDeviceCacheArmed -Armed $false

    $hasBootloaderIdentity = -not [string]::IsNullOrWhiteSpace($BootloaderVid) -and -not [string]::IsNullOrWhiteSpace($BootloaderPid)
    $sameUsbIdentity = $hasBootloaderIdentity -and
        -not [string]::IsNullOrWhiteSpace($RuntimeVid) -and
        -not [string]::IsNullOrWhiteSpace($RuntimePid) -and
        ((Normalize-NiusUsbId -Value $BootloaderVid) -eq (Normalize-NiusUsbId -Value $RuntimeVid)) -and
        ((Normalize-NiusUsbId -Value $BootloaderPid) -eq (Normalize-NiusUsbId -Value $RuntimePid))

    Write-NiusTiming ('touch mode start: {0}' -f $touchMode.Label)
        $touchAttempt = Touch-SerialPort1200 `
            -PortName $PortName `
            -ObservePostTouchResetCycle:$sameUsbIdentity
        if (-not $touchAttempt.Triggered) {
            if ($touchAttempt.FailureKind -eq 'busy') {
                return [pscustomobject]@{
                    Triggered = $false
                    Candidate = $touchMode
                    Transition = $null
                    FailureKind = 'busy'
                    Error = $touchAttempt.Error
                }
            }
            # A disappearing USB CDC can leave SerialPort.Dispose blocked even
            # though the DTR touch reached the MCU. Before declaring failure,
            # check the exact board scope for the transition produced by
            # that timed-out helper.
            if ($hasBootloaderIdentity) {
                try {
                    $lateEvidence = Wait-AdafruitBootloaderTransition `
                        -PortName $PortName `
                        -BootloaderVid $BootloaderVid `
                        -BootloaderPid $BootloaderPid `
                        -RuntimeVid $RuntimeVid `
                        -RuntimePid $RuntimePid `
                        -InterfaceParentPrefix $InterfaceParentPrefix `
                        -PreferredCompositeStableId $PreferredCompositeStableId `
                        -ExpectedLabel $ExpectedLabel `
                        -ExpectedModel $ExpectedModel `
                        -ExpectedBoardId $ExpectedBoardId `
                        -TimeoutMs $perModeTimeoutMs `
                        -Purpose ($touchMode.Label + ' late transition')
                    Write-NiusTiming ('touch mode confirmed after bounded helper timeout: {0}' -f $touchMode.Label)
                    return [pscustomobject]@{
                        Triggered = $true
                        Candidate = $touchMode
                        Transition = $lateEvidence
                        FailureKind = ''
                        Error = ''
                    }
                }
                catch {
                }
            }
            return [pscustomobject]@{
                Triggered = $false
                Candidate = $touchMode
                Transition = $null
                FailureKind = $touchAttempt.FailureKind
                Error = $touchAttempt.Error
            }
        }

        if ($touchAttempt.SawResetCycle -and
            (-not $hasBootloaderIdentity -or
             [string]::IsNullOrWhiteSpace($ExpectedLabel))) {
            # Auto mode does not know the bootloader VID/PID until after the
            # reset. A confirmed detach is enough to prove the touch occurred;
            # the caller immediately performs the exact post-touch identity
            # discovery. Waiting for the old runtime COM to return would be
            # wrong when the bootloader owns a different COM number.
            Write-NiusDetail ('[nius] CDC reset-cycle observed inline during {0}; continuing to post-touch identity discovery.' -f $touchMode.Label) -ForegroundColor DarkGray
            Write-NiusTiming ('touch mode confirmed inline: {0}' -f $touchMode.Label)
            return [pscustomobject]@{
                Triggered = $true
                Candidate = $touchMode
                Transition = $null
                FailureKind = ''
                Error = ''
            }
        }
        elseif ($touchAttempt.SawResetCycle) {
            Write-NiusDetail ('[nius] CDC detached during {0}; waiting for the selected board scoped UF2 identity.' -f $touchMode.Label) -ForegroundColor DarkGray
        }

        try {
            $transitionEvidence = $null
            if ($hasBootloaderIdentity) {
                $transitionEvidence = Wait-AdafruitBootloaderTransition `
                    -PortName $PortName `
                    -BootloaderVid $BootloaderVid `
                    -BootloaderPid $BootloaderPid `
                    -RuntimeVid $RuntimeVid `
                    -RuntimePid $RuntimePid `
                    -InterfaceParentPrefix $InterfaceParentPrefix `
                    -PreferredCompositeStableId $PreferredCompositeStableId `
                    -ExpectedLabel $ExpectedLabel `
                    -ExpectedModel $ExpectedModel `
                    -ExpectedBoardId $ExpectedBoardId `
                    -TimeoutMs $perModeTimeoutMs `
                    -Purpose $touchMode.Label
            }
            else {
                Wait-SerialPortResetCycle -PortName $PortName -Purpose $touchMode.Label
            }

            Write-NiusTiming ('touch mode confirmed: {0}' -f $touchMode.Label)
            return [pscustomobject]@{
                Triggered = $true
                Candidate = $touchMode
                Transition = $transitionEvidence
                FailureKind = ''
                Error = ''
            }
        }
        catch {
            Write-NiusTiming ('touch mode timed out: {0}' -f $touchMode.Label)
            if ($hasBootloaderIdentity) {
                Write-NiusDetail ('[nius] {0} on {1} produced no scoped bootloader transition: {2}. The caller will perform one final identity check before deciding.' -f $touchMode.Label, $PortName, $_.Exception.Message) -ForegroundColor DarkGray
                Write-NiusDetail '[nius] Skipping a duplicate touch after a successful pulse; the board may already be re-enumerating.' -ForegroundColor DarkGray
                return [pscustomobject]@{
                    Triggered = $false
                    Candidate = $null
                    Transition = $null
                    FailureKind = 'transition-unconfirmed'
                    Error = $_.Exception.Message
                }
            }
            else {
                Write-NiusDetail ('[warn] {0} on {1} did not produce a confirmed bootloader transition: {2}' -f $touchMode.Label, $PortName, $_.Exception.Message) -ForegroundColor DarkYellow
            }
        }
    return [pscustomobject]@{
        Triggered = $false
        Candidate = $null
        Transition = $null
        FailureKind = 'touch-failed'
        Error = 'no touch mode produced a scoped bootloader transition'
    }
}

function Get-NiusPostTouchSleepMilliseconds {
    # Buttonless boards: give USB detach / bootloader enumerate before nrfutil grabs COM.
    # NOTE: this is now used as the *ceiling* of the adaptive settle wait
    # (Wait-NiusBootloaderPortSettled), not a blind fixed sleep, so a fast
    # board no longer pays the full window.
    $ms = 3800
    $o = $env:NIUS_POST_TOUCH_SLEEP_MS
    if (-not [string]::IsNullOrWhiteSpace($o)) {
        $p = -1
        if ([int]::TryParse($o, [ref]$p) -and $p -ge 500 -and $p -le 15000) {
            $ms = $p
        }
    }
    return $ms
}

function Wait-NiusBootloaderPortSettled {
    # Adaptive replacement for the blind post-touch sleep.
    #
    # After a confirmed 1200 bps touch the board's runtime CDC detaches and the
    # Adafruit serial-DFU CDC re-enumerates on the same COM name. The hazard is
    # a race: the *dying* runtime port can still look openable for a few hundred
    # ms before it drops, and grabbing it then makes nrfutil fail mid-transfer.
    #
    # Instead of always sleeping the full ceiling, we proceed as soon as the COM
    # has been *continuously* openable for StableMs (so a flicker on the dying
    # port resets the streak and we re-acquire on the real bootloader port),
    # never returning before FloorMs (USB line-coding settle) and never waiting
    # past CeilingMs. Typical fast boards settle in ~1 s instead of 3.8 s.
    param(
        [string]$PortName,
        [int]$CeilingMs = 3800,
        [int]$FloorMs = 400,
        [int]$StableMs = 350
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        # No concrete port to probe; fall back to a short blind settle.
        Start-Sleep -Milliseconds ([Math]::Min($CeilingMs, 1500))
        return
    }
    if ($CeilingMs -lt $FloorMs) { $FloorMs = $CeilingMs }

    $start = Get-Date
    $floorDeadline = $start.AddMilliseconds($FloorMs)
    $ceilDeadline = $start.AddMilliseconds($CeilingMs)
    $openSince = $null
    while ((Get-Date) -lt $ceilDeadline) {
        $state = Get-SerialPortUsableState -PortName $PortName
        if ($state.Present -and $state.Openable) {
            if ($null -eq $openSince) {
                $openSince = Get-Date
            }
            elseif ((((Get-Date) - $openSince).TotalMilliseconds -ge $StableMs) -and ((Get-Date) -ge $floorDeadline)) {
                $elapsed = [int]((Get-Date) - $start).TotalMilliseconds
                Write-NiusDetail ('[nius] Bootloader COM {0} settled in {1} ms (adaptive; ceiling {2} ms).' -f $PortName, $elapsed, $CeilingMs) -ForegroundColor DarkGray
                return
            }
        }
        else {
            # Detach / not-openable: reset the stability streak so we only
            # accept the port once the re-enumeration has actually completed.
            $openSince = $null
        }
        Start-Sleep -Milliseconds 120
    }
    Write-NiusDetail ('[nius] Bootloader COM {0} did not stabilize within ceiling {1} ms; proceeding anyway.' -f $PortName, $CeilingMs) -ForegroundColor DarkYellow
}

# Check serial port presence via WMI without opening the port.
# This is safe to call during 1200bps touch wait loops because it never sends
# SET_LINE_CODING to the CDC device (which would cancel the pending touch).
function Test-SerialPortPnpPresent {
    param([string]$PortName)
    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        return $false
    }
    $normalizedPort = $PortName.Trim().ToUpperInvariant()
    # Fast presence check via GetPortNames() (an instant registry read of
    # HKLM\HARDWARE\DEVICEMAP\SERIALCOMM) instead of the ~1-3 s Win32_SerialPort
    # WMI query. This runs in the tight 70 ms detach/reattach poll, so the WMI
    # cost there used to slow the effective poll to seconds and miss the brief
    # same-PID re-enumeration, forcing a full touch-mode timeout.
    foreach ($n in [System.IO.Ports.SerialPort]::GetPortNames()) {
        if (([string]$n).Trim().ToUpperInvariant() -eq $normalizedPort) {
            return $true
        }
    }
    return $false
}

function Get-SerialPortUsableState {
    param([string]$PortName)

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        return [pscustomobject]@{
            Present = $false
            Openable = $false
            Issue = 'invalid port name'
        }
    }

    $normalizedPort = $PortName.Trim().ToUpperInvariant()
    # Fast presence via GetPortNames() (see Test-SerialPortPnpPresent); the
    # openability probe below is what actually matters and is unchanged.
    $present = $false
    foreach ($n in [System.IO.Ports.SerialPort]::GetPortNames()) {
        if (([string]$n).Trim().ToUpperInvariant() -eq $normalizedPort) {
            $present = $true
            break
        }
    }
    if (-not $present) {
        return [pscustomobject]@{
            Present = $false
            Openable = $false
            Issue = 'port not enumerated'
        }
    }

    $serial = $null
    try {
        $serial = New-Object System.IO.Ports.SerialPort $PortName, 115200, 'None', 8, 'One'
        $serial.DtrEnable = $false
        $serial.RtsEnable = $false
        $serial.ReadTimeout = 100
        $serial.WriteTimeout = 100
        $serial.Open()
        $serial.Close()
        return [pscustomobject]@{
            Present = $true
            Openable = $true
            Issue = ''
        }
    }
    catch {
        return [pscustomobject]@{
            Present = $true
            Openable = $false
            Issue = $_.Exception.Message
        }
    }
    finally {
        if ($null -ne $serial) {
            try {
                $serial.Dispose()
            }
            catch {
            }
        }
    }
}

function Wait-SerialPortResetCycle {
    param(
        [string]$PortName,
        [int]$DetachTimeoutMs = 5000,
        [int]$ReattachTimeoutMs = 12000,
        [string]$Purpose = 'USB reset cycle'
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        throw ('No concrete port selected for {0}.' -f $Purpose)
    }

    Write-NiusDetail ('[nius] Waiting for {0} on {1} - expecting COM to detach and re-enumerate after 1200 bps touch.' -f $Purpose, $PortName) -ForegroundColor DarkGray
    $detachDeadline = (Get-Date).AddMilliseconds($DetachTimeoutMs)
    $lastIssue = 'port stayed present'
    $sawDetach = $false
    while ((Get-Date) -lt $detachDeadline) {
        # PnP-only: do NOT open the port - opening at 115200 baud would send
        # SET_LINE_CODING(115200) to the firmware and cancel the pending 1200bps touch.
        if (-not (Test-SerialPortPnpPresent -PortName $PortName)) {
            $sawDetach = $true
            $lastIssue = 'port detached'
            break
        }
        Start-Sleep -Milliseconds 160
    }

    if (-not $sawDetach) {
        throw ('Port never detached after touch ({0}).' -f $lastIssue)
    }

    Wait-SerialPortReady -PortName $PortName -Purpose $Purpose -TimeoutMs $ReattachTimeoutMs
}

function Wait-SerialPortReady {
    param(
        [string]$PortName,
        [int]$TimeoutMs = 12000,
        [string]$Purpose = 'serial control port'
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        throw ('No concrete port selected for {0}.' -f $Purpose)
    }

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    $waitStarted = Get-Date
    $normalizedPort = $PortName.Trim().ToUpperInvariant()
    $lastIssue = 'port not enumerated'
    $progressEveryMs = 2500
    $nextProgressAt = $waitStarted.AddMilliseconds($progressEveryMs)
    Write-NiusDetail ('[nius] Waiting for {0} ({1}) - COM must open cleanly (timeout {2} ms). Windows may hold the port briefly after reset.' -f $Purpose, $PortName, $TimeoutMs) -ForegroundColor DarkGray
    while ((Get-Date) -lt $deadline) {
        $loopSleep = 280
        $state = Get-SerialPortUsableState -PortName $PortName
        if ($state.Present) {
            if ($state.Openable) {
                return
            }
            $lastIssue = $state.Issue
            $loopSleep = 520
            $lm = $lastIssue.ToLowerInvariant()
            if ($lm -match 'denied|being used|sharing violation|access|busy|refused|\breject') {
                $loopSleep = 900
            }
        }

        if ((Get-Date) -ge $nextProgressAt) {
            $elapsed = [int](((Get-Date) - $waitStarted).TotalMilliseconds)
            Write-NiusDetail ('[nius]   serial wait {0} ms / {1} ms - last: {2}' -f $elapsed, $TimeoutMs, $lastIssue) -ForegroundColor DarkGray
            $nextProgressAt = (Get-Date).AddMilliseconds($progressEveryMs)
        }

        Start-Sleep -Milliseconds $loopSleep
    }

    throw ('Timed out waiting for {0} {1}: {2}' -f $Purpose, $PortName, $lastIssue)
}

function Wait-AdafruitBootloaderTransition {
    param(
        [string]$PortName,
        [string]$BootloaderVid,
        [string]$BootloaderPid,
        [string]$RuntimeVid = '',
        [string]$RuntimePid = '',
        [string]$InterfaceParentPrefix = '',
        [string]$PreferredCompositeStableId = '',
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = '',
        [int]$TimeoutMs = 12000,
        [string]$Purpose = 'bootloader transition'
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        throw ('No concrete port selected for {0}.' -f $Purpose)
    }

    Write-NiusDetail ('[nius] Waiting for {0} on {1} - accepting either COM reset-cycle or MSC/UF2 bootloader evidence.' -f $Purpose, $PortName) -ForegroundColor DarkGray
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    $waitStarted = Get-Date
    $nextProgressAt = $waitStarted.AddMilliseconds(2500)
    $lastIssue = 'port stayed present'
    $lastSnapshot = $null
    $sawDetach = $false
    # Give the cheap serial/UF2 checks a full second before asking Windows for a
    # comparatively expensive PnP inventory. Otherwise a slow provider can
    # block this loop past its deadline and hide an already-mounted UF2 volume.
    $lastSnapshotAt = Get-Date
    $normalizedPort = $PortName.Trim().ToUpperInvariant()
    $expectsUf2 = -not [string]::IsNullOrWhiteSpace($ExpectedLabel) -or
        -not [string]::IsNullOrWhiteSpace($ExpectedModel) -or
        -not [string]::IsNullOrWhiteSpace($ExpectedBoardId)
    $hasRuntimeIdentity = -not [string]::IsNullOrWhiteSpace($RuntimeVid) -and -not [string]::IsNullOrWhiteSpace($RuntimePid)
    $differentUsbIdentity = $hasRuntimeIdentity -and
        ((Normalize-NiusUsbId -Value $BootloaderVid) -ne (Normalize-NiusUsbId -Value $RuntimeVid) -or
         (Normalize-NiusUsbId -Value $BootloaderPid) -ne (Normalize-NiusUsbId -Value $RuntimePid))

    while ((Get-Date) -lt $deadline) {
        # PnP-only: do NOT open the port - opening at 115200 baud would send
        # SET_LINE_CODING(115200) to the firmware and cancel the pending 1200bps touch.
        # Use the INSTANT, always-fresh GetPortNames() (reads SERIALCOMM) rather
        # than the cached Win32_SerialPort inventory: the cache, once populated,
        # returned a frozen "present" for the whole loop so the brief same-PID
        # detach was never seen and the reset-cycle never fired -> the touch
        # mode timed out and we fell through to a slow retry. GetPortNames is
        # sub-millisecond, so the fast poll genuinely catches the detach.
        $portPresent = (@([System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object { $_.Trim().ToUpperInvariant() }) -contains $normalizedPort)
        if (-not $portPresent) {
            $sawDetach = $true
            $lastIssue = 'port detached'
        }

        # Primary, cheap signal: detached then back == bootloader re-enumerated.
        if ($sawDetach -and $portPresent) {
            return [pscustomobject]@{
                Mode = 'reset-cycle'
                Summary = if ($lastSnapshot) { $lastSnapshot.Summary } else { '' }
                Uf2 = $null
            }
        }

        # Secondary signal for boards that don't show a clean detach. Check the
        # stable-ID-scoped UF2 volume before the USB-tree/WMI snapshot: the
        # latter can take many seconds when Windows has stale device nodes.
        $uf2Probe = $null
        if ($expectsUf2) {
            try {
                $uf2Probe = Get-Uf2ProbeSummary -ExpectedLabel $ExpectedLabel -ExpectedModel $ExpectedModel -ExpectedBoardId $ExpectedBoardId -PreferredCompositeStableId $PreferredCompositeStableId
            }
            catch {
                $uf2Probe = $null
            }
            if ($null -ne $uf2Probe) {
                return [pscustomobject]@{
                    Mode = 'bootloader-evidence'
                    Summary = if ($lastSnapshot) { $lastSnapshot.Summary } else { 'scoped UF2 volume matched' }
                    Uf2 = $uf2Probe
                }
            }
        }

        # For serial-only bootloaders, poll the bounded registry snapshot at
        # ~1 Hz. Explicit UF2 paths use their stronger matched-volume evidence.
        if (((Get-Date) - $lastSnapshotAt).TotalMilliseconds -ge 1000) {
            $lastSnapshotAt = Get-Date
            # A full Win32_PnPEntity snapshot can block for tens of seconds on
            # a stale USB child and overrun this function's millisecond timeout.
            # The serial registry already provides the exact present COM,
            # VID/PID, and composite stable identity needed for a serial-only
            # bootloader transition, so keep this wait genuinely bounded.
            $strongBootloaderEvidence = $false
            if (-not $expectsUf2 -and $differentUsbIdentity) {
                $fastBootloader = Get-NiusFastUsbSerialRegistrySnapshot `
                    -Vid $BootloaderVid `
                    -ProductId $BootloaderPid `
                    -PreferredCompositeStableId $PreferredCompositeStableId
                if ($fastBootloader.Available) {
                    $strongBootloaderEvidence = @($fastBootloader.Matches).Count -gt 0
                    $lastSnapshot = [pscustomobject]@{
                        Summary = if ($strongBootloaderEvidence) {
                            'scoped bootloader CDC matched registry identity'
                        } else {
                            'bootloader CDC not present in registry snapshot'
                        }
                    }
                }
            }
            if ($strongBootloaderEvidence) {
                return [pscustomobject]@{
                    Mode = 'bootloader-evidence'
                    Summary = $lastSnapshot.Summary
                    Uf2 = $null
                }
            }
        }

        if ((Get-Date) -ge $nextProgressAt) {
            $elapsed = [int](((Get-Date) - $waitStarted).TotalMilliseconds)
            $summary = if ($lastSnapshot) { $lastSnapshot.Summary } else { 'snapshot unavailable' }
            Write-NiusDetail ('[nius]   bootloader wait {0} ms / {1} ms - last: {2}; snapshot: {3}' -f $elapsed, $TimeoutMs, $lastIssue, $summary) -ForegroundColor DarkGray
            $nextProgressAt = (Get-Date).AddMilliseconds(2500)
        }

        # Poll fast: a same-PID clone can detach and re-enumerate the bootloader
        # CDC in well under 200 ms, and missing that brief detach is what used to
        # force a full-timeout fallback to the next touch mode.
        Start-Sleep -Milliseconds 70
    }

    $summary = if ($lastSnapshot) { $lastSnapshot.Summary } else { 'snapshot unavailable' }
    throw ('No scoped bootloader transition observed after touch ({0}); snapshot: {1}' -f $lastIssue, $summary)
}

$script:NiusUploadCompositeStableId = ''
$toolPath = if ($Mode -eq 'jlink') {
    Resolve-NiusJLinkExe -Preferred $Tool
}
else {
    [System.IO.Path]::GetFullPath($Tool)
}
try {
    Assert-ToolExists -Path $toolPath
    Write-Banner -BoardName $Board
    Write-NiusTiming 'banner done'

    # Validate the complete linked image before inspecting, opening, touching,
    # or resetting any USB interface. A wrong layout, corrupt HEX record, bad
    # vector table, or oversized image must leave the running board untouched.
    $enterBootloaderOnlyRequested = (($EnterBootloaderOnly -eq 'true') -or ($EnterBootloaderOnly -eq '1'))
    if ($Mode -in @('dfu', 'openocd', 'jlink') -and -not $enterBootloaderOnlyRequested) {
        Assert-NiusApplicationImage `
            -HexPath $Hex `
            -FamilyId $Uf2FamilyId `
            -AppStart $Uf2AppStart `
            -MaxSize $MaximumSize `
            -RamEnd $RamEnd
        Write-NiusTiming 'application image preflight done'
    }

    # Arm the PnP-device snapshot cache for the pre-touch identity/port checks
    # (board is on a stable, not-yet-touched port set). Disarmed before the
    # 1200 touch (next to Clear-SerialPortInventoryCache) so transition polling
    # re-queries fresh. Saves several seconds of repeated ~2-3 s enumerations.
    Set-PnpDeviceCacheArmed -Armed $true

    # --- Concurrency guard ----------------------------------------------------
    # Robustness: the user may click Upload again while an upload is still in
    # flight (e.g. mid-1200-touch or mid-DFU). A second instance racing for the
    # same COM would interleave touches / DFU frames and can corrupt flash. A
    # per-port system-wide mutex makes the first upload the sole owner; a
    # duplicate fails fast BEFORE any touch/port access, so it cannot
    # disturb the in-flight transfer. The OS releases the mutex when the owning
    # process exits (covers crashes / killed nrfutil); a later acquirer that
    # sees the abandoned state simply takes ownership. Tune the contention wait via
    # NIUS_UPLOAD_LOCK_WAIT_MS (default 600 ms: instant when free, brief enough
    # to fail fast on a real concurrent upload).
    $script:NiusUploadMutex = $null
    $script:NiusUploadMutexHeld = $false
    $portKey = 'default'
    if ($Port -match '^(?i)COM\d+$') {
        $portKey = $Port.Trim().ToUpperInvariant()
    }
    elseif (-not [string]::IsNullOrWhiteSpace($Board)) {
        $portKey = ($Board.Trim() -replace '[^A-Za-z0-9_]', '_')
    }
    $mutexName = 'Global\NiusUpload_' + $portKey
    $lockWaitMs = 600
    $o = $env:NIUS_UPLOAD_LOCK_WAIT_MS
    if (-not [string]::IsNullOrWhiteSpace($o)) {
        $p = -1
        if ([int]::TryParse($o, [ref]$p) -and $p -ge 0 -and $p -le 10000) { $lockWaitMs = $p }
    }
    try {
        $script:NiusUploadMutex = New-Object System.Threading.Mutex($false, $mutexName)
    }
    catch {
        try {
            $mutexName = 'Local\NiusUpload_' + $portKey
            $script:NiusUploadMutex = New-Object System.Threading.Mutex($false, $mutexName)
        }
        catch {
            Throw-NiusUploadFailure (New-UploadFailure -Kind 'generic' -ExitCode 1 -Output ('Could not create the upload lock for {0}; no USB action was attempted.' -f $portKey) -Exe $toolPath)
        }
    }
    if ($script:NiusUploadMutex) {
        try {
            $script:NiusUploadMutexHeld = $script:NiusUploadMutex.WaitOne($lockWaitMs)
        }
        catch [System.Threading.AbandonedMutexException] {
            # Previous owner exited without releasing (crash / kill). The
            # wait still grants us ownership; proceed.
            $script:NiusUploadMutexHeld = $true
        }
        if (-not $script:NiusUploadMutexHeld) {
            Throw-NiusUploadFailure (New-UploadFailure -Kind 'generic' -ExitCode 1 -Output (@(
                        ('Another upload is already in progress on {0}; ignoring this duplicate request.' -f $portKey),
                        'Wait for the current upload to finish (the board will reboot into the new firmware), then upload again.',
                        'ZH: This port already has an upload in progress; the duplicate click was ignored. Wait for it to finish, then retry.'
                    ) -join ' ') -Exe $toolPath)
        }
    }
    # --------------------------------------------------------------------------

    # --- Host-wide DFU serialization -----------------------------------------
    # The per-port mutex above only stops a duplicate upload to the SAME port.
    # Two uploads to different boards can still collide because they share the
    # host's DFU tooling and both trigger USB re-enumeration. The uploader never
    # kills a process owned by another live program; this mutex prevents those
    # operations from overlapping in the first place.
    # This host-wide mutex serializes the whole USB-sensitive upload across
    # boards: a second board's upload WAITS for the first to finish instead of
    # racing it. Orphan cleanup is PID-scoped and never terminates a process
    # whose parent is still alive. Tune the wait via
    # NIUS_UPLOAD_HOST_LOCK_WAIT_MS (default 300000 = 5 min, long enough to queue
    # behind a real other-board upload; on timeout the new request fails closed
    # before touching USB).
    $script:NiusHostUploadMutex = $null
    $script:NiusHostUploadMutexHeld = $false
    $hostWaitMs = 300000
    $hw = $env:NIUS_UPLOAD_HOST_LOCK_WAIT_MS
    if (-not [string]::IsNullOrWhiteSpace($hw)) {
        $hp = -1
        if ([int]::TryParse($hw, [ref]$hp) -and $hp -ge 0 -and $hp -le 600000) { $hostWaitMs = $hp }
    }
    try {
        $script:NiusHostUploadMutex = New-Object System.Threading.Mutex($false, 'Global\NiusUpload_HostDfu')
    }
    catch {
        try {
            $script:NiusHostUploadMutex = New-Object System.Threading.Mutex($false, 'Local\NiusUpload_HostDfu')
        }
        catch {
            Throw-NiusUploadFailure (New-UploadFailure -Kind 'generic' -ExitCode 1 -Output 'Could not create the host DFU lock; no USB action was attempted.' -Exe $toolPath)
        }
    }
    if ($script:NiusHostUploadMutex) {
        $waitStart = Get-Date
        try {
            $script:NiusHostUploadMutexHeld = $script:NiusHostUploadMutex.WaitOne($hostWaitMs)
        }
        catch [System.Threading.AbandonedMutexException] {
            # Previous owner exited without releasing (crash / kill); we own it.
            $script:NiusHostUploadMutexHeld = $true
        }
        if (-not $script:NiusHostUploadMutexHeld) {
            Throw-NiusUploadFailure (New-UploadFailure -Kind 'generic' -ExitCode 1 -Output 'Another board upload still owns the host DFU lock. This upload was cancelled before touching USB.' -Exe $toolPath)
        }
        elseif (((Get-Date) - $waitStart).TotalMilliseconds -ge 250) {
            Write-NiusDetail '[nius] waited for another board''s upload to finish (host-wide DFU serialization).' -ForegroundColor DarkGray
        }
    }
    # --------------------------------------------------------------------------

    # --- Yield request to a running USB-CDC GDB-stub bridge -------------------
    # Upload-during-debug: a debug session's bridge holds the service COM, so our
    # 1200-touch can't reach the board. Drop a request file; the bridge releases
    # the port and exits (uploading new firmware ends the debug session anyway).
    # Then wait briefly for the COM to become openable. Harmless when no bridge
    # is running. The board's halted-stub touch handler reboots a *paused* debug
    # target into the bootloader once the touch lands. Disable with
    # NIUS_DISABLE_BRIDGE_YIELD=1.
    $script:NiusBridgeYieldFile = $null
    if ($env:NIUS_DISABLE_BRIDGE_YIELD -ne '1' -and $Port -match '^(?i)COM\d+$') {
        $yieldKey = $Port.Trim().ToUpperInvariant()
        $script:NiusBridgeYieldFile = Join-Path $env:TEMP ('nius_gdb_yield_{0}.req' -f $yieldKey)
        try {
            Set-Content -LiteralPath $script:NiusBridgeYieldFile -Value ('pid={0} utc={1:o}' -f $PID, [datetime]::UtcNow) -Encoding ascii -ErrorAction SilentlyContinue
        }
        catch {
        }
        # Both bridge implementations poll this request every 50 ms. Give them
        # a bounded handoff window, then let the already-bounded touch runspace
        # prove that the port can open. Probing here with SerialPort.Open() was
        # unsafe: Windows can block that call for ~30 s after re-enumeration,
        # turning an otherwise healthy upload into a random long wait.
        Start-Sleep -Milliseconds 200
    }
    # --------------------------------------------------------------------------

    Write-Section -Label 'transport handshake initialized'
    $expectedRuntimeIdentity = Resolve-ExpectedRuntimeUsbIdentity -BoardName $Board
    Write-NiusTiming 'runtime identity resolved'
    $effectiveRuntimeUsbVid = if ($expectedRuntimeIdentity) { [string]$expectedRuntimeIdentity.Vid } else { '' }
    $effectiveRuntimeUsbPid = if ($expectedRuntimeIdentity) { [string]$expectedRuntimeIdentity.Pid } else { '' }
    if (-not [string]::IsNullOrWhiteSpace($RuntimeUsbVid) -and $RuntimeUsbVid -notlike '{*}') {
        $effectiveRuntimeUsbVid = $RuntimeUsbVid.Trim()
    }
    if (-not [string]::IsNullOrWhiteSpace($RuntimeUsbPid) -and $RuntimeUsbPid -notlike '{*}') {
        $effectiveRuntimeUsbPid = $RuntimeUsbPid.Trim()
        if ([string]::IsNullOrWhiteSpace($effectiveRuntimeUsbVid) -and -not [string]::IsNullOrWhiteSpace($UsbVid) -and $UsbVid -ne 'auto') {
            $effectiveRuntimeUsbVid = $UsbVid.Trim()
        }
        $expectedRuntimeIdentity = [pscustomobject]@{
            Vid = $effectiveRuntimeUsbVid
            Pid = $effectiveRuntimeUsbPid
        }
    }
    $runtimePortsBeforeUpload = @(
        Get-NiusRuntimeComNamesForIdentity `
            -RuntimeVid $effectiveRuntimeUsbVid `
            -RuntimePid $effectiveRuntimeUsbPid
    )
    $adafruitControlPort = $Port
    $controlPortAlreadyBootloader = $false
    $bootloaderTransitionConfirmed = $false
    $explicitUf2UploadMode = ($BootloaderMode -eq 'uf2')
    $uf2AlreadyMountedSummary = $null
    if (-not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbVid) -and -not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbPid)) {
        Write-NiusTiming 'port resolution start'
        $portResolution = Resolve-AdafruitSerialControlPort -SelectedPort $Port -RuntimeVid $effectiveRuntimeUsbVid -RuntimePid $effectiveRuntimeUsbPid
        if (-not $portResolution) {
            $portResolution = Resolve-AdafruitSerialControlPortWithBoardIdentity -SelectedPort $Port -BoardName $Board
        }
        if ($portResolution -and -not [string]::IsNullOrWhiteSpace($portResolution.Port)) {
            $remapWouldChange = ($portResolution.Port.Trim().ToUpperInvariant() -ne $Port.Trim().ToUpperInvariant())
            $strictRejectUserCdc = ($env:NIUS_REJECT_USER_CDC_UPLOAD_PORT -eq '1')
            if (($BootloaderMode -eq 'adafruit-dfu' -or $BootloaderMode -eq 'nordic-dfu') -and
                $remapWouldChange -and $strictRejectUserCdc) {
                $svc = $portResolution.Port
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'adafruit-dfu' -ExitCode 1 -Output (@(
                        'Wrong COM for Adafruit serial DFU: selected port is the Arduino USER CDC (Serial.print) interface, not the SERVICE / maintenance CDC used for 1200bps touch and nrfutil.',
                        ('Selected={0}; use SERVICE CDC instead (typically smaller COM index / MI_00): {1}.' -f $Port, $svc),
                        ('ZH: USER CDC serial cannot flash firmware; switch IDE Tools->Port to SERVICE CDC (MI_00): {0}' -f $svc),
                        'Unset NIUS_REJECT_USER_CDC_UPLOAD_PORT to allow the identity-scoped SERVICE CDC remap.'
                    ) -join ' ') -Exe $toolPath)
            }
            $adafruitControlPort = $portResolution.Port
            if ($adafruitControlPort -ne $Port -and $portResolution) {
                Write-NiusDetail ('[nius] serial DFU control port remap: selected {0}, using {1} ({2})' -f $Port, $adafruitControlPort, $portResolution.Reason)
            }
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($adafruitControlPort) -and $adafruitControlPort.Trim().ToUpperInvariant() -eq $Port.Trim().ToUpperInvariant()) {
        $boardPortResolution = Resolve-AdafruitSerialControlPortWithBoardIdentity -SelectedPort $Port -BoardName $Board
        if ($boardPortResolution -and -not [string]::IsNullOrWhiteSpace($boardPortResolution.Port) -and
            $boardPortResolution.Port.Trim().ToUpperInvariant() -ne $Port.Trim().ToUpperInvariant()) {
            $adafruitControlPort = $boardPortResolution.Port
            Write-NiusDetail ('[nius] serial DFU control port remap (board identity): selected {0}, using {1} ({2})' -f $Port, $adafruitControlPort, $boardPortResolution.Reason)
        }
    }
    # If the selected IDE port was the USER CDC, the first yield request above
    # targeted that interface. The debug bridge owns the resolved SERVICE CDC,
    # so request a second, identity-scoped handoff before trying the 1200-bps
    # touch. This never opens, resets, or rebinds either Windows device.
    $script:NiusRemappedBridgeYieldFile = $null
    if ($env:NIUS_DISABLE_BRIDGE_YIELD -ne '1' -and
        -not [string]::IsNullOrWhiteSpace($adafruitControlPort) -and
        $adafruitControlPort -match '^(?i)COM\d+$' -and
        $adafruitControlPort.Trim().ToUpperInvariant() -ne $Port.Trim().ToUpperInvariant()) {
        $yieldKey = $adafruitControlPort.Trim().ToUpperInvariant()
        $script:NiusRemappedBridgeYieldFile = Join-Path $env:TEMP ('nius_gdb_yield_{0}.req' -f $yieldKey)
        try {
            Set-Content -LiteralPath $script:NiusRemappedBridgeYieldFile -Value ('pid={0} utc={1:o}' -f $PID, [datetime]::UtcNow) -Encoding ascii -ErrorAction SilentlyContinue
        }
        catch {
        }
        Start-Sleep -Milliseconds 200
    }
    $runtimeSharesUploadIdentity = $false
    $adafruitControlPortParentPrefix = ''
    $adafruitControlPortCompositeStableId = ''
    if ($Mode -eq 'dfu') {
        $fastControlSnapshot = Get-NiusFastUsbSerialRegistrySnapshot `
            -Vid $effectiveRuntimeUsbVid -ProductId $effectiveRuntimeUsbPid
        $fastControlMatch = @($fastControlSnapshot.Matches | Where-Object {
                ([string]$_.DeviceID).Trim().ToUpperInvariant() -eq $adafruitControlPort.Trim().ToUpperInvariant()
            } | Select-Object -First 1)
        if ($fastControlSnapshot.Available -and $fastControlMatch.Count -eq 1) {
            $adafruitControlPortParentPrefix = [string]$fastControlMatch[0].InterfaceParentPrefix
            $adafruitControlPortCompositeStableId = [string]$fastControlMatch[0].CompositeStableId
        }
        else {
            $adafruitControlPortParentPrefix = Get-SerialPortUsbInterfaceParentInstancePrefix -PortName $adafruitControlPort
            $adafruitControlPortCompositeStableId = Get-SerialPortUsbParentCompositeStableId -PortName $adafruitControlPort
        }
        $script:NiusUploadCompositeStableId = $adafruitControlPortCompositeStableId
        if (-not [string]::IsNullOrWhiteSpace($adafruitControlPort) -and
            [string]::IsNullOrWhiteSpace($adafruitControlPortParentPrefix) -and
            [string]::IsNullOrWhiteSpace($adafruitControlPortCompositeStableId)) {
            if ($explicitUf2UploadMode) {
                try {
                    $uf2AlreadyMountedSummary = Get-Uf2ProbeSummary -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId
                }
                catch {
                    $uf2AlreadyMountedSummary = $null
                }
            }

            if ($uf2AlreadyMountedSummary) {
                $UseTouch1200 = 'false'
                $controlPortAlreadyBootloader = $true
                Write-NiusDetail ('[nius] Selected upload port "{0}" is not present, but a single matching UF2 drive is already mounted at {1}; using UF2 directly.' -f $adafruitControlPort, $uf2AlreadyMountedSummary.Drive) -ForegroundColor DarkGray
            }
            else {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'port' -ExitCode 1 -Output (@(
                            ('Selected upload port "{0}" is not present as a USB serial device.' -f $adafruitControlPort),
                            'For UF2 uploads, upload.ps1 can continue from an already-mounted UF2 drive only when exactly one matching UF2 volume is visible.',
                            'Re-select the board current SERVICE/DFU port in Arduino IDE Tools->Port, or leave only the target board mounted in UF2 mode and upload again.'
                        ) -join ' ') -Exe $toolPath)
            }
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbVid) -and -not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbPid) -and
        -not [string]::IsNullOrWhiteSpace($UsbVid) -and -not [string]::IsNullOrWhiteSpace($UsbPid) -and
        $UsbVid -ne 'auto' -and $UsbPid -ne 'auto') {
        $runtimeSharesUploadIdentity =
            ($effectiveRuntimeUsbVid.Trim().ToUpperInvariant() -eq $UsbVid.Trim().ToUpperInvariant()) -and
            ($effectiveRuntimeUsbPid.Trim().ToUpperInvariant() -eq $UsbPid.Trim().ToUpperInvariant())
    }
    if ($BootloaderMode -eq 'adafruit-dfu') {
        $controlPortAlreadyBootloader = Test-SerialPortMatchesUsbIdentity -PortName $adafruitControlPort -Vid $UsbVid -ProductId $UsbPid
        # Same-PID clone boards are ambiguous: the selected COM can already be the
        # runtime service CDC even though VID/PID still match the bootloader.
        # Only trust "already bootloader" when there is stronger evidence than
        # the COM's PNPDeviceID alone (e.g. MSC/UF2 sibling still present).
        if ($runtimeSharesUploadIdentity -and $controlPortAlreadyBootloader) {
            $samePidSnapshot = Get-AdafruitRuntimeSnapshot -BootloaderVid $UsbVid -BootloaderPid $UsbPid -RuntimeVid $effectiveRuntimeUsbVid -RuntimePid $effectiveRuntimeUsbPid -InterfaceParentPrefix $adafruitControlPortParentPrefix
            $uf2Probe = $null
            if ($explicitUf2UploadMode) {
                try {
                    $uf2Probe = Get-Uf2ProbeSummary -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId -PreferredCompositeStableId $adafruitControlPortCompositeStableId
                }
                catch {
                    $uf2Probe = $null
                }
            }
            # Do not trust storage-only PnP siblings here. Legacy same-PID no-SD
            # boards can keep an MI_02 USB Mass Storage node present in runtime,
            # which makes the selected service CDC look like an already-running
            # bootloader and causes us to skip the required 1200-bps touch. A
            # matched UF2 volume is the only reliable pre-touch signal that the
            # selected board is already in bootloader.
            $strongBootloaderEvidence = $explicitUf2UploadMode -and ($null -ne $uf2Probe)
            if ($strongBootloaderEvidence) {
                Write-NiusDetail ('[nius] Same-PID upload path: bootloader evidence present on {0}; skipping 1200 touch ({1})' -f $adafruitControlPort, $samePidSnapshot.Summary) -ForegroundColor DarkGray
            }
            else {
                $controlPortAlreadyBootloader = $false
                Write-NiusDetail ('[nius] Same-PID upload path: treating {0} as runtime service CDC and keeping 1200 touch enabled.' -f $adafruitControlPort) -ForegroundColor DarkGray
            }
        }
    }

    Write-NiusTiming 'identity/bootloader checks done'
    # All PnP-heavy pre-touch identity checks are done; disarm the snapshot cache
    # (path-independent) so the connect/touch/transition phase re-queries fresh.
    Set-PnpDeviceCacheArmed -Armed $false
    if ($Mode -eq 'dfu') {
        Assert-InputArtifact -Path $Hex -Label 'hex'

        # Single visible "Connecting" line; everything between here and the
        # byte-progress bar (1200 bps touch, bootloader enumerate, COM settle)
        # is internal and stays verbose-only so the console reads as
        # Connecting -> [transfer bar] -> Upload complete.
        Write-Stage -Percent 0 -Label 'Connecting'

        $autoMode = ($BootloaderMode -eq 'auto' -or $UsbVid -eq 'auto' -or $UsbPid -eq 'auto')
        if ($autoMode) {
            Write-NiusDetail '[nius] Auto-detecting bootloader...' -ForegroundColor DarkGray
            # A selected COM that still has the declared runtime identity is
            # authoritative application state. Scanning every supported
            # bootloader before touching it is both redundant and slow on a
            # Windows host with many USB devices. Touch first, then identify
            # the exact bootloader that actually appeared.
            $selectedPortIsRuntime =
                $UseTouch1200 -eq 'true' -and
                -not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbVid) -and
                -not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbPid) -and
                (Test-SerialPortMatchesUsbIdentity `
                    -PortName $adafruitControlPort `
                    -Vid $effectiveRuntimeUsbVid `
                    -ProductId $effectiveRuntimeUsbPid)
            if ($selectedPortIsRuntime) {
                $resolved = [pscustomobject]@{
                    Resolved = $false
                    ProbedCandidates = 'selected port is the declared runtime endpoint; bootloader probe deferred until after touch'
                }
                Write-NiusDetail '[nius] Selected port is the runtime endpoint; deferring bootloader discovery until after its reset transition.' -ForegroundColor DarkGray
            }
            else {
                $resolved = Resolve-AutoBootloader -InterfaceParentPrefix $adafruitControlPortParentPrefix -PreferredCompositeStableId $adafruitControlPortCompositeStableId -Attempts 2 -DelayMs 200
            }
            # VID:PID 239A:00B3 is shared by several clone bootloaders whose
            # application starts differ. Neither a PnP hit nor dfu-util listing
            # that identity proves the flash layout. When the candidate default
            # conflicts with the compiled layout and no INFO_UF2 evidence is
            # mounted, enter the selected board's bootloader first and read its
            # authoritative metadata before deciding.
            if ($resolved.Resolved -and $UseTouch1200 -eq 'true') {
                $initialCompiledStart = Normalize-NiusHexAddress -Value $Uf2AppStart
                $initialResolvedStart = Normalize-NiusHexAddress -Value $resolved.AppStart
                $initialHasUf2Metadata = -not [string]::IsNullOrWhiteSpace([string]$resolved.DriveRoot)
                if ($resolved.Vid -eq '0x239a' -and
                    $resolved.Pid -eq '0x00b3' -and
                    -not $initialHasUf2Metadata -and
                    -not [string]::IsNullOrWhiteSpace($initialCompiledStart) -and
                    -not [string]::IsNullOrWhiteSpace($initialResolvedStart) -and
                    $initialCompiledStart -ne $initialResolvedStart) {
                    Write-NiusDetail ('[nius] auto-detect: ambiguous 239A:00B3 layout {0} conflicts with compiled {1}; requesting scoped bootloader metadata before deciding.' -f $initialResolvedStart, $initialCompiledStart) -ForegroundColor DarkGray
                    $resolved = [pscustomobject]@{
                        Resolved = $false
                        ProbedCandidates = 'ambiguous 239A:00B3 bootloader identity; INFO_UF2.TXT required'
                    }
                }
            }
            if (-not $resolved.Resolved -and $UseTouch1200 -eq 'true') {
                Stop-NiusLingeringAdafruitNrfutil -Phase touch
                Write-NiusDetail '[nius] Entering bootloader (1200 bps touch)...' -ForegroundColor DarkGray
                $touchTransition = Invoke-Touch1200Transition `
                    -PortName $adafruitControlPort `
                    -RuntimeVid $effectiveRuntimeUsbVid `
                    -RuntimePid $effectiveRuntimeUsbPid `
                    -InterfaceParentPrefix $adafruitControlPortParentPrefix `
                    -PreferredCompositeStableId $adafruitControlPortCompositeStableId `
                    -ExpectedLabel $Uf2VolumeLabel `
                    -ExpectedModel $Uf2Model `
                    -ExpectedBoardId $Uf2BoardId
                if ($touchTransition.Triggered) {
                    $bootloaderTransitionConfirmed = $true
                }
                if (-not $touchTransition.Triggered) {
                    Write-NiusDetail ('[warn] 1200 bps touch on {0} did not confirm a host-visible transition; probing scoped bootloader anyway (same-PID clones may keep the COM name).' -f $adafruitControlPort) -ForegroundColor DarkYellow
                }
                # Transition detection already waited for runtime detach. Give
                # Windows only a short scheduling grace, then let the bounded
                # identity probe itself wait for the bootloader. A second blind
                # multi-second sleep only duplicates that wait.
                Start-Sleep -Milliseconds 150
                $resolved = Resolve-AutoBootloader -InterfaceParentPrefix $adafruitControlPortParentPrefix -PreferredCompositeStableId $adafruitControlPortCompositeStableId -Attempts 10 -DelayMs 350
                if (-not $resolved.Resolved -and -not $touchTransition.Triggered) {
                    Throw-NiusUploadFailure (New-UploadFailure -Kind 'adafruit-dfu' -ExitCode 1 -Output ('Unable to trigger 1200 bps touch on {0}; the service/user CDC port may be missing or busy.' -f $adafruitControlPort) -Exe $toolPath)
                }
            }
            if (-not $resolved.Resolved) {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'dfu-wait' -ExitCode 1 -Output ('Auto-detect probed for known nRF52 bootloaders ({0}); none visible on host. Check that the board is plugged in, that the user firmware honors 1200 bps touch, and that no other process is holding the COM port open.' -f $resolved.ProbedCandidates) -Exe $toolPath)
            }
            Write-NiusDetail ('[nius] auto-detect resolved to {0} ({1}:{2}, {3})' -f $resolved.Kind.ToUpper(), $resolved.Vid, $resolved.Pid, $resolved.Note)
            $compiledAppStart = Normalize-NiusHexAddress -Value $Uf2AppStart
            $resolvedAppStart = Normalize-NiusHexAddress -Value $resolved.AppStart
            if (-not [string]::IsNullOrWhiteSpace($compiledAppStart) -and
                -not [string]::IsNullOrWhiteSpace($resolvedAppStart) -and
                $compiledAppStart -ne $resolvedAppStart) {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'layout' -ExitCode 1 -Output (@(
                            ('Auto-detect found a bootloader layout at app start {0}, but the selected Arduino IDE option compiled this sketch for {1}.' -f $resolvedAppStart, $compiledAppStart),
                            ('Detected: {0}:{1}; {2}' -f $resolved.Vid, $resolved.Pid, $resolved.Note),
                            'Arduino compiles before upload, so upload.ps1 cannot move an already-linked image to a different app start.',
                            'Select a Bootloader / DFU option with the matching layout and compile again. For no-SoftDevice / nice!nano-style clone firmware, choose a no SoftDevice / MBR only (0x1000) option.',
                            'ZH: Auto-detect found a bootloader app start that differs from this compile option. Select the matching 0x1000/0x26000/0x27000 Bootloader / DFU menu, then compile and upload again.'
                        ) -join [Environment]::NewLine) -Exe $toolPath)
            }
            $BootloaderMode = $resolved.Kind
            $UsbVid = $resolved.Vid
            $UsbPid = $resolved.Pid
            $Uf2FamilyId = $resolved.Family
            $Uf2VolumeLabel = $resolved.VolumeLabel
            $Uf2Model = $resolved.Model
            $Uf2BoardId = $resolved.BoardId
            $runtimeSharesUploadIdentity =
                (-not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbVid)) -and
                (-not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbPid)) -and
                ($effectiveRuntimeUsbVid.Trim().ToUpperInvariant() -eq $UsbVid.Trim().ToUpperInvariant()) -and
                ($effectiveRuntimeUsbPid.Trim().ToUpperInvariant() -eq $UsbPid.Trim().ToUpperInvariant())
            # Touch already attempted (or skipped because the device was already in
            # bootloader mode); don't re-touch on the legacy path below.
            $UseTouch1200 = 'false'
        }

        if ($BootloaderMode -eq 'uf2' -or $BootloaderMode -eq 'adafruit-dfu') {
            Assert-InputArtifact -Path $Hex -Label 'hex'
        } else {
            Assert-InputArtifact -Path $Bin -Label 'bin'
        }

        if ($BootloaderMode -eq 'uf2' -and -not [string]::IsNullOrWhiteSpace($adafruitControlPortCompositeStableId)) {
            try {
                $scopedUf2AlreadyMounted = Get-Uf2ProbeSummary -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId -PreferredCompositeStableId $adafruitControlPortCompositeStableId
                if ($scopedUf2AlreadyMounted) {
                    $uf2AlreadyMountedSummary = $scopedUf2AlreadyMounted
                    $UseTouch1200 = 'false'
                    $controlPortAlreadyBootloader = $true
                    Write-NiusDetail ('[nius] Selected board already has a matching UF2 drive mounted at {0}; skipping 1200 touch.' -f $uf2AlreadyMountedSummary.Drive) -ForegroundColor DarkGray
                }
            }
            catch {
            }
        }

        $touchPrepared = $false
        $touchDetectedBootloader = $null
        if ($BootloaderMode -eq 'adafruit-dfu' -and $UseTouch1200 -eq 'true' -and $controlPortAlreadyBootloader) {
            Write-NiusDetail '[nius] Entering bootloader (1200 bps touch)...' -ForegroundColor DarkGray
        } elseif ($UseTouch1200 -eq 'true') {
            Stop-NiusLingeringAdafruitNrfutil -Phase touch
            if ($runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu') {
                Write-NiusDetail '[nius] Entering bootloader (1200 bps touch)...' -ForegroundColor DarkGray
            }
            else {
                Write-NiusDetail '[nius] Entering bootloader (1200 bps touch)...' -ForegroundColor DarkGray
            }
            $touchTransitionResult = Invoke-Touch1200Transition `
                -PortName $adafruitControlPort `
                -BootloaderVid $UsbVid `
                -BootloaderPid $UsbPid `
                -RuntimeVid $effectiveRuntimeUsbVid `
                -RuntimePid $effectiveRuntimeUsbPid `
                -InterfaceParentPrefix $adafruitControlPortParentPrefix `
                -PreferredCompositeStableId $adafruitControlPortCompositeStableId `
                -ExpectedLabel $(if ($BootloaderMode -eq 'uf2') { $Uf2VolumeLabel } else { '' }) `
                -ExpectedModel $(if ($BootloaderMode -eq 'uf2') { $Uf2Model } else { '' }) `
                -ExpectedBoardId $(if ($BootloaderMode -eq 'uf2') { $Uf2BoardId } else { '' })
            if ($touchTransitionResult.Triggered) {
                $touchPrepared = $true
                $bootloaderTransitionConfirmed = $true
                if ($touchTransitionResult.PSObject.Properties['Transition'] -and
                    $touchTransitionResult.Transition -and
                    $touchTransitionResult.Transition.PSObject.Properties['Uf2'] -and
                    $touchTransitionResult.Transition.Uf2) {
                    $touchDetectedBootloader = [pscustomobject]@{
                        Kind = 'uf2'
                        Summary = $touchTransitionResult.Transition.Uf2
                    }
                }
                if ($touchTransitionResult.Candidate) {
                    Write-NiusDetail ('[nius] 1200 bps touch path confirmed via {0}.' -f $touchTransitionResult.Candidate.Label) -ForegroundColor DarkGray
                }
            }
            else {
                if ($touchTransitionResult.PSObject.Properties['FailureKind'] -and
                    $touchTransitionResult.FailureKind -eq 'busy') {
                    Throw-NiusUploadFailure (New-UploadFailure -Kind 'adafruit-dfu' -ExitCode 1 -Output ('Could not open port {0}: access is denied because another process owns the selected runtime/service CDC. No bootloader request or flash operation was attempted. Close only that serial monitor or console and retry.' -f $adafruitControlPort) -Exe $toolPath)
                }
                if ($runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu') {
                    Write-NiusDetail ('[nius] Same-PID 1200 bps touch on {0} produced no host-visible transition; the identity-scoped resolver must still prove a bootloader CDC before transfer.' -f $adafruitControlPort) -ForegroundColor DarkGray
                }
                else {
                    Write-NiusDetail ('[warn] Unable to confirm 1200 bps touch on {0}; the identity-scoped resolver must prove an already-running bootloader before transfer.' -f $adafruitControlPort) -ForegroundColor DarkYellow
                }
            }
        } else {
            Write-NiusDetail '[nius] 1200 bps touch skipped; using already-detected bootloader state.' -ForegroundColor DarkGray
        }

        if ($touchPrepared) {
            if ($BootloaderMode -eq 'uf2') {
                # The transition gate already observed this board's scoped UF2
                # volume. Its runtime COM is expected to remain absent until
                # after the file copy, so waiting for that COM here is both
                # unnecessary and logically impossible.
                Write-NiusDetail '[nius] Scoped UF2 volume is ready; skipping serial-port settle.' -ForegroundColor DarkGray
            }
            elseif ($BootloaderMode -eq 'adafruit-dfu') {
                # The runtime COM normally disappears and the bootloader gets a
                # different COM number. Waiting for the old name can only burn
                # the whole settle ceiling. The exact bootloader resolver below
                # remaps by composite identity and waits on the resulting port.
                Write-NiusDetail '[nius] Runtime CDC detached; resolving the bootloader control COM directly.' -ForegroundColor DarkGray
            }
            else {
                # Adaptive settle: proceed the moment the bootloader COM is
                # stably openable instead of always burning the full window.
                Wait-NiusBootloaderPortSettled -PortName $adafruitControlPort -CeilingMs (Get-NiusPostTouchSleepMilliseconds)
            }
            Write-NiusTiming 'connect: touch + settle done'
        }
        if ($touchPrepared -and $BootloaderMode -eq 'adafruit-dfu') {
            Write-NiusDetail '[nius] DFU: progress ~90% only means nrfutil is in serial DFU wait/transfer (host-side); MCU may still be in application if 1200/DTR reset did not arm yet).' -ForegroundColor DarkGray
        }

        if ($BootloaderMode -eq 'uf2' -and $UseTouch1200 -eq 'true' -and
            -not $bootloaderTransitionConfirmed -and -not $controlPortAlreadyBootloader) {
            $scopedUf2AfterUnconfirmedTouch = $null
            if (-not [string]::IsNullOrWhiteSpace($adafruitControlPortCompositeStableId)) {
                try {
                    $scopedUf2AfterUnconfirmedTouch = Get-Uf2ProbeSummary -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId -PreferredCompositeStableId $adafruitControlPortCompositeStableId
                }
                catch {
                    $scopedUf2AfterUnconfirmedTouch = $null
                }
            }

            if ($scopedUf2AfterUnconfirmedTouch) {
                $bootloaderTransitionConfirmed = $true
                $controlPortAlreadyBootloader = $true
                Write-NiusDetail ('[nius] 1200 bps touch did not show a COM detach, but a matching UF2 drive is mounted at {0} for selected board serial {1}; continuing.' -f $scopedUf2AfterUnconfirmedTouch.Drive, $adafruitControlPortCompositeStableId) -ForegroundColor DarkGray
            }
            else {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'uf2-wait' -ExitCode 1 -Output (@(
                            ('1200 bps touch on {0} did not confirm that the selected board entered UF2 bootloader.' -f $adafruitControlPort),
                            'Refusing to use an unscoped UF2 drive already mounted on the host, because another board may be in DFU mode.',
                            'Select the target board current SERVICE/DFU port in Arduino IDE Tools->Port, or retry after only the target board is mounted as UF2.'
                        ) -join ' ') -Exe $toolPath)
            }
        }

        if ($enterBootloaderOnlyRequested) {
            if ($BootloaderMode -ne 'uf2') {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'uf2-wait' -ExitCode 1 -Output ('Enter UF2 drive only was selected, but the resolved bootloader mode is "{0}". Select a UF2 bootloader entry, or use Auto-detect with a UF2-capable board.' -f $BootloaderMode) -Exe $toolPath)
            }
            Write-Stage -Percent 32 -Label 'Connecting'
            $uf2Ready = Wait-ForUsbBootloader `
                -Exe $toolPath `
                -UsbVid $UsbVid `
                -UsbPid $UsbPid `
                -ExpectedLabel $Uf2VolumeLabel `
                -ExpectedModel $Uf2Model `
                -ExpectedBoardId $Uf2BoardId `
                -PreferredCompositeStableId $adafruitControlPortCompositeStableId `
                -ExpectUf2 $true
            if (-not $uf2Ready -or $uf2Ready.Kind -ne 'uf2' -or -not $uf2Ready.Summary) {
                $problemReports = @(Get-Uf2MassStorageProblemReports -UsbVid $UsbVid -UsbPid $UsbPid -PreferredCompositeStableId $adafruitControlPortCompositeStableId -InterfaceParentPrefix $adafruitControlPortParentPrefix)
                $lines = New-Object 'System.Collections.Generic.List[string]'
                $lines.Add(('Selected board on {0} entered bootloader request, but no matching UF2 drive was mounted. Expected label "{1}", model "{2}", board-id "{3}".' -f $adafruitControlPort, $Uf2VolumeLabel, $Uf2Model, $Uf2BoardId))
                foreach ($problem in $problemReports) {
                    $lines.Add($problem)
                }
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'uf2-wait' -ExitCode 1 -Output ($lines.ToArray() -join [Environment]::NewLine) -Exe $toolPath)
            }
            Write-NiusBootloaderReady -Drive $uf2Ready.Summary.Drive -Note ('Matched board serial: {0}' -f $adafruitControlPortCompositeStableId)
            exit 0
        }

        # adafruit-dfu programs over the CDC interface; the host-side MSC LUN
        # is occasionally not promoted to a DiskDrive on Windows, but the CDC
        # path is unaffected. adafruit-nrfutil opens the COM port directly, so
        # we skip Wait-ForUsbBootloader / dfu-util probes for serial transports.
        # Nordic's PCA10059 bootloader uses the same Secure DFU packet protocol
        # over USB CDC, not the USB DFU class implemented by dfu-util.
        if ($BootloaderMode -eq 'adafruit-dfu' -or $BootloaderMode -eq 'nordic-dfu') {
            if ($BootloaderMode -eq 'adafruit-dfu' -and $runtimeSharesUploadIdentity -and
                -not $bootloaderTransitionConfirmed -and -not $controlPortAlreadyBootloader) {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'dfu-wait' -ExitCode 1 -Output ('The runtime and bootloader share one VID/PID, but neither a reset transition nor stronger identity-scoped bootloader evidence was proven for {0}. Refusing a speculative protocol probe on the application COM; no mutating transfer was launched.' -f $adafruitControlPort) -Exe $toolPath)
            }
            $initialSdReq = Resolve-AdafruitInitialSdReq -SdReq $SdReq
            $bootloaderPortResolution = Wait-AdafruitBootloaderControlPort `
                -SelectedPort $Port `
                -CurrentPort $adafruitControlPort `
                -BootloaderVid $UsbVid `
                -BootloaderPid $UsbPid `
                -PreferredCompositeStableId $adafruitControlPortCompositeStableId `
                -InterfaceParentPrefix $adafruitControlPortParentPrefix `
                -TimeoutMs (Get-NiusAdafruitSerialReadyMilliseconds)
            if ($bootloaderPortResolution -and $bootloaderPortResolution.Resolved -and -not [string]::IsNullOrWhiteSpace($bootloaderPortResolution.Port)) {
                $resolvedBootloaderPort = $bootloaderPortResolution.Port.Trim()
                if ($resolvedBootloaderPort.ToUpperInvariant() -ne $adafruitControlPort.Trim().ToUpperInvariant()) {
                    Write-NiusDetail ('[nius] serial DFU bootloader control port remap: using {0} instead of {1} ({2})' -f $resolvedBootloaderPort, $adafruitControlPort, $bootloaderPortResolution.Reason) -ForegroundColor DarkGray
                }
                $adafruitControlPort = $resolvedBootloaderPort
            }
            else {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'dfu-wait' -ExitCode 1 -Output ('No identity-scoped bootloader service CDC was proven within the bounded wait: {0}. Refusing to open the old runtime COM or another attached board; no mutating transfer was launched.' -f $bootloaderPortResolution.Reason) -Exe $toolPath)
            }

            # Serial-only bootloaders need no mass-storage interface.  When the
            # selected physical board also exposes INFO_UF2.TXT, use it as an
            # immediate, identity-scoped link-layout check without adding a
            # mount wait to the normal serial-DFU path.
            if (-not [string]::IsNullOrWhiteSpace($adafruitControlPortCompositeStableId)) {
                $scopedUf2ForSerialDfu = Get-Uf2ProbeSummary `
                    -ExpectedLabel $Uf2VolumeLabel `
                    -ExpectedModel $Uf2Model `
                    -ExpectedBoardId $Uf2BoardId `
                    -PreferredCompositeStableId $adafruitControlPortCompositeStableId
                if ($scopedUf2ForSerialDfu) {
                    Assert-Uf2BuildLayoutMatchesBootloader `
                        -Uf2Summary $scopedUf2ForSerialDfu `
                        -ExpectedAppStart $Uf2AppStart `
                        -Context 'Selected serial-DFU bootloader'
                }
            }

            # A serial-DFU child can erase or write before it reports a failure.
            # Retouching, repeating, or changing sd-req after that boundary is not
            # demonstrably idempotent, particularly for single-bank bootloaders.
            # Run exactly one identity-bound transfer and preserve any failure for
            # explicit recovery instead of guessing that a retry is safe.
            Write-NiusDetail '[nius] Starting one identity-bound Adafruit serial DFU transfer...' -ForegroundColor DarkGray
            Invoke-AdafruitDfuDeploy -HexPath $Hex -Port $adafruitControlPort -SdReq $initialSdReq
            # One scoped guard is sufficient: it requires the expected runtime
            # VID/PID and preserved composite identity. It deliberately does
            # not open the fresh application COM, which would compete with the
            # user's Serial Monitor and can block while usbser retires DFU.
            Write-Stage -Percent 94 -Label 'Verifying'
            $null = Invoke-NiusMisflashGuardAfterSamePidUpload `
                -PortName $adafruitControlPort `
                -FallbackRuntimePortName $Port `
                -RuntimeVid $effectiveRuntimeUsbVid `
                -RuntimePid $effectiveRuntimeUsbPid `
                -BootloaderVid $UsbVid `
                -BootloaderPid $UsbPid `
                -RuntimePortsBefore $runtimePortsBeforeUpload `
                -ExpectedAppStart $Uf2AppStart `
                -ExpectedLabel $Uf2VolumeLabel `
                -ExpectedModel $Uf2Model `
                -ExpectedBoardId $Uf2BoardId `
                -PreferredCompositeStableId $adafruitControlPortCompositeStableId
            Write-NiusUploadComplete
            exit 0
        }

        $detectedBootloader = $null
        if ($WaitForUploadPort -eq 'true') {
            Write-Stage -Percent 32 -Label 'Connecting'
            if ($BootloaderMode -eq 'uf2' -and $touchDetectedBootloader) {
                $detectedBootloader = $touchDetectedBootloader
                Write-NiusTiming 'final bootloader discovery reused touch evidence'
            }
            else {
                Write-NiusTiming 'final bootloader discovery start'
                $detectedBootloader = Wait-ForUsbBootloader -Exe $toolPath -UsbVid $UsbVid -UsbPid $UsbPid -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId -PreferredCompositeStableId $adafruitControlPortCompositeStableId -ExpectUf2 ($BootloaderMode -eq 'uf2')
                Write-NiusTiming 'final bootloader discovery done'
            }
            if (-not $detectedBootloader) {
                if ($BootloaderMode -eq 'uf2') {
                    Write-Stage -Percent 68 -Label 'Uploading'
                    if (Invoke-Uf2SerialDfuFallback `
                            -HexPath $Hex `
                            -SelectedPort $Port `
                            -CurrentPort $adafruitControlPort `
                            -BootloaderVid $UsbVid `
                            -BootloaderPid $UsbPid `
                            -PreferredCompositeStableId $adafruitControlPortCompositeStableId `
                            -InterfaceParentPrefix $adafruitControlPortParentPrefix `
                            -SdReq $SdReq `
                            -ExpectedAppStart $Uf2AppStart `
                            -ExpectedLabel $Uf2VolumeLabel `
                            -ExpectedModel $Uf2Model `
                            -ExpectedBoardId $Uf2BoardId) {
                        Write-Stage -Percent 94 -Label 'Verifying'
                        $postFallbackState = Wait-ForAdafruitRuntimeTransition `
                            -BootloaderVid $UsbVid `
                            -BootloaderPid $UsbPid `
                            -RuntimeVid $effectiveRuntimeUsbVid `
                            -RuntimePid $effectiveRuntimeUsbPid `
                            -InterfaceParentPrefix $adafruitControlPortParentPrefix `
                            -PreferredCompositeStableId $adafruitControlPortCompositeStableId
                        if (-not $postFallbackState.Success) {
                            Throw-NiusUploadFailure (New-UploadFailure -Kind 'post-verify' -ExitCode 1 -Output ('uf2_serial_fallback=used; {0}' -f $postFallbackState.Summary) -Exe $toolPath)
                        }
                        Write-NiusUploadComplete -Note 'UF2 drive was unavailable; used bootloader serial DFU fallback.'
                        exit 0
                    }
                }
                $waitFailureKind = if ($BootloaderMode -eq 'uf2') { 'uf2-wait' } else { 'dfu-wait' }
                $waitFailureOutput = if ($BootloaderMode -eq 'uf2') {
                    $problemReports = @(Get-Uf2MassStorageProblemReports -UsbVid $UsbVid -UsbPid $UsbPid -PreferredCompositeStableId $adafruitControlPortCompositeStableId -InterfaceParentPrefix $adafruitControlPortParentPrefix)
                    $lines = New-Object 'System.Collections.Generic.List[string]'
                    $lines.Add(('Selected board on {0} did not present a matching UF2 volume after 1200 bps touch. Expected label "{1}", model "{2}", board-id "{3}".' -f $adafruitControlPort, $Uf2VolumeLabel, $Uf2Model, $Uf2BoardId))
                    foreach ($problem in $problemReports) {
                        $lines.Add($problem)
                    }
                    ($lines.ToArray() -join [Environment]::NewLine)
                }
                else {
                    ''
                }
                Throw-NiusUploadFailure (New-UploadFailure -Kind $waitFailureKind -ExitCode 1 -Output $waitFailureOutput -Exe $toolPath)
            }
        } else {
            Write-Stage -Percent 32 -Label 'Connecting'
            $detectedBootloader = [pscustomobject]@{
                Kind = if ($BootloaderMode -eq 'uf2') { 'uf2' } else { 'dfu' }
                Summary = if ($BootloaderMode -eq 'uf2') { Get-Uf2ProbeSummary -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId -PreferredCompositeStableId $adafruitControlPortCompositeStableId } else { $null }
            }
        }

        if ($BootloaderMode -eq 'uf2') {
            if ($detectedBootloader.Kind -ne 'uf2') {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'dfu' -ExitCode 1 -Output ('Selected bootloader mode requires UF2, but the board presented Nordic DFU {0}:{1} instead.' -f $UsbVid, $UsbPid) -Exe $toolPath)
            }

            Write-Stage -Percent 68 -Label 'Uploading'
            Assert-Uf2BuildLayoutMatchesBootloader -Uf2Summary $detectedBootloader.Summary -ExpectedAppStart $Uf2AppStart -Context 'Selected UF2 bootloader'
            Invoke-Uf2Deploy `
                -HexPath $Hex `
                -FamilyId $Uf2FamilyId `
                -AppStart $Uf2AppStart `
                -MaxSize $MaximumSize `
                -RamEnd $RamEnd `
                -DrivePath $detectedBootloader.Summary.Drive
            Write-Stage -Percent 94 -Label 'Verifying'
            $null = Invoke-NiusMisflashGuardAfterSamePidUpload `
                -PortName $adafruitControlPort `
                -FallbackRuntimePortName $Port `
                -RuntimeVid $effectiveRuntimeUsbVid `
                -RuntimePid $effectiveRuntimeUsbPid `
                -BootloaderVid $UsbVid `
                -BootloaderPid $UsbPid `
                -RuntimePortsBefore $runtimePortsBeforeUpload `
                -ExpectedAppStart $Uf2AppStart `
                -ExpectedLabel $Uf2VolumeLabel `
                -ExpectedModel $Uf2Model `
                -ExpectedBoardId $Uf2BoardId `
                -PreferredCompositeStableId $adafruitControlPortCompositeStableId
            Write-NiusUploadComplete
            exit 0
        }

        # Every packaged USB mode is handled above: UF2 mass storage or Secure
        # serial DFU (Adafruit and Nordic). The old generic dfu-util fallback
        # could never be selected by boards.txt and used the wrong transport
        # for Nordic's CDC Secure DFU bootloader, so fail closed if a future
        # recipe forgets to declare a supported mode.
        Throw-NiusUploadFailure (New-UploadFailure -Kind 'dfu' -ExitCode 1 -Output ('Unsupported USB bootloader mode after discovery: {0}' -f $BootloaderMode) -Exe $toolPath)
    }

    if ($Mode -eq 'bootloader') {
        Invoke-NiusBootloaderDeploy `
            -OpenOcdExe $toolPath `
            -ScriptRootPath $ScriptRoot `
            -OpenOcdConfig $Config `
            -BootloaderHexPath $Hex `
            -Protocol $ProgrammerProtocol `
            -Device $JLinkDevice `
            -TargetFlashEnd $FlashEnd `
            -TargetRamEnd $RamEnd
        exit 0
    }

    if ($Mode -eq 'jlink') {
        Invoke-JLinkDeploy -JLinkExe $toolPath -HexPath $Hex -Device $JLinkDevice
        exit 0
    }

    Assert-InputArtifact -Path $Hex -Label 'hex'
    Write-Stage -Percent 10 -Label 'Connecting'
    Write-Stage -Percent 42 -Label 'Uploading'
    $resolvedHex = (Resolve-Path -LiteralPath $Hex).Path
    $openOcdCommand = 'init; halt; program {0} verify reset exit' -f `
        (ConvertTo-OpenOcdTclWord -Value $resolvedHex)
    $openOcdArguments = New-NiusOpenOcdArguments `
        -ScriptRootPath $ScriptRoot `
        -ConfigPath $Config `
        -Command $openOcdCommand
    Invoke-CommandChecked -Exe $toolPath -Arguments $openOcdArguments -FailureKind 'openocd' -ProgressPercent 76 -ProgressLabel 'SWD flash transaction active'
    Write-NiusUploadComplete
}
catch {
    $failure = $_.Exception.Message
    if ($_.Exception -is [System.Management.Automation.RuntimeException] -and $_.Exception.ErrorRecord -and $_.Exception.ErrorRecord.TargetObject -and $_.Exception.ErrorRecord.TargetObject.PSTypeNames -contains 'NiusUploadFailure') {
        $report = $_.Exception.ErrorRecord.TargetObject
        Show-FailureReport -Title $report.Kind -Summary $report.Summary -Hints $report.Hints -Details $report.Details
        exit 1
    }

    if ($_.TargetObject -and $_.TargetObject.PSTypeNames -contains 'NiusUploadFailure') {
        $report = $_.TargetObject
        Show-FailureReport -Title $report.Kind -Summary $report.Summary -Hints $report.Hints -Details $report.Details
        exit 1
    }

    Show-FailureReport -Title 'generic' -Summary $failure -Hints @(Get-FailureHints -Kind 'generic' -Output $failure) -Details @(Get-RelevantLogLines -Output $failure)
    exit 1
}
finally {
    # Remove the bridge-yield request so a bridge started later doesn't see a
    # stale request and release its port. (It is also freshness-gated.)
    $bridgeYieldVar = Get-Variable -Name NiusBridgeYieldFile -Scope Script -ErrorAction SilentlyContinue
    if ($bridgeYieldVar -and $script:NiusBridgeYieldFile) {
        try { Remove-Item -LiteralPath $script:NiusBridgeYieldFile -Force -ErrorAction SilentlyContinue } catch {}
    }
    $remappedBridgeYieldVar = Get-Variable -Name NiusRemappedBridgeYieldFile -Scope Script -ErrorAction SilentlyContinue
    if ($remappedBridgeYieldVar -and $script:NiusRemappedBridgeYieldFile) {
        try { Remove-Item -LiteralPath $script:NiusRemappedBridgeYieldFile -Force -ErrorAction SilentlyContinue } catch {}
    }
    # Best-effort release of the concurrency lock on the normal-completion path.
    # On `exit`, crash, or kill the OS releases the named mutex automatically, so
    # a held lock never wedges future uploads even if this block is skipped.
    $mutexHeldVar = Get-Variable -Name NiusUploadMutexHeld -Scope Script -ErrorAction SilentlyContinue
    $mutexVar = Get-Variable -Name NiusUploadMutex -Scope Script -ErrorAction SilentlyContinue
    if ($mutexHeldVar -and $mutexVar -and $script:NiusUploadMutexHeld -and $script:NiusUploadMutex) {
        try { $script:NiusUploadMutex.ReleaseMutex() } catch {}
    }
    if ($mutexVar -and $script:NiusUploadMutex) {
        try { $script:NiusUploadMutex.Dispose() } catch {}
    }
    # Release the host-wide DFU serialization mutex so the next board's upload
    # (which may be blocked in WaitOne) can proceed. As above, the OS releases
    # it automatically on exit/crash, so a queued upload never wedges forever.
    $hostHeldVar = Get-Variable -Name NiusHostUploadMutexHeld -Scope Script -ErrorAction SilentlyContinue
    $hostVar = Get-Variable -Name NiusHostUploadMutex -Scope Script -ErrorAction SilentlyContinue
    if ($hostHeldVar -and $hostVar -and $script:NiusHostUploadMutexHeld -and $script:NiusHostUploadMutex) {
        try { $script:NiusHostUploadMutex.ReleaseMutex() } catch {}
    }
    if ($hostVar -and $script:NiusHostUploadMutex) {
        try { $script:NiusHostUploadMutex.Dispose() } catch {}
    }
}
