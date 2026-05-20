param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('dfu', 'openocd')]
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
    [string]$Hex = '',
    [AllowEmptyString()]
    [string]$UsbVid = '',
    [AllowEmptyString()]
    [string]$UsbPid = '',
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
    [string]$Board = 'nrf52',
    [AllowEmptyString()]
    [string]$BootloaderMode = 'nordic-dfu',
    [AllowEmptyString()]
    [string]$Uf2FamilyId = '',
    [AllowEmptyString()]
    [string]$Uf2AppStart = '0x0',
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

    # NiusRobotLab "shadow" figlet (embossed / yin-yang effect), pure ASCII
    # (only _ | \ / ( ) ` chars), so it renders identically in every host
    # regardless of console codepage. Single-quoted literals keep backtick
    # and backslash literal. Printed exactly once, right after compile,
    # before the upload progress. Bracketed by *** rule lines so it stands
    # apart from the surrounding compiler / IDE output.
    Write-NiusHostLine '*****************************************************************'
    Write-NiusHostLine '  \  |_)             _ \        |           |   |           |    '
    Write-NiusHostLine '   \ | | |   |  __| |   |  _ \  __ \   _ \  __| |      _` | __ \ '
    Write-NiusHostLine ' |\  | | |   |\__ \ __ <  (   | |   | (   | |   |     (   | |   |'
    Write-NiusHostLine '_| \_|_|\__,_|____/_| \_\\___/ _.__/ \___/ \__|_____|\__,_|_.__/ '
    Write-NiusHostLine '*****************************************************************'
    Write-NiusHostLine ('   nRF52 Flash Console   ***   Target: {0}' -f $BoardName)
    Write-NiusHostLine '*****************************************************************'

    # Mark the start of the user-visible upload run for the closing summary.
    $script:NiusUploadStartUtc = [datetime]::UtcNow
}

# Closing summary: the final 100% bar plus 2 lines of plain-language status
# (total time, soft reset), framed by a *** rule so it stands apart from the
# IDE's own "uploading done" chatter. Pure ASCII.
function Write-NiusUploadComplete {
    param([string]$Note = '')

    Write-Stage -Percent 100 -Label 'Upload complete'

    $elapsed = ''
    if ($script:NiusUploadStartUtc) {
        $sec = [Math]::Round(([datetime]::UtcNow - $script:NiusUploadStartUtc).TotalSeconds, 1)
        $elapsed = '{0}s' -f $sec
    }
    Write-NiusHostLine '*****************************************************************'
    if (-not [string]::IsNullOrWhiteSpace($elapsed)) {
        Write-NiusHostLine ('  Total upload time : {0}' -f $elapsed)
    }
    Write-NiusHostLine '  Soft reset        : done - board rebooted into new firmware'
    if (-not [string]::IsNullOrWhiteSpace($Note)) {
        Write-NiusHostLine ('  {0}' -f $Note)
    }
    Write-NiusHostLine '*****************************************************************'
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
    #   NIUS |==============>.......|  64%  Uploading  29.0/45.0 KB
    # Pure ASCII so it renders identically regardless of console codepage.
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
    $line = '  NIUS |{0}| {1,3}%  {2}' -f $bar, $clampedPercent, $Label
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

function ConvertTo-ProcessArguments {
    param([string[]]$Arguments)

    return (($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') {
            '"{0}"' -f ($_.Replace('"', '\"'))
        } else {
            $_
        }
    }) -join ' ')
}

function Assert-ToolExists {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw ('Upload tool not found: {0}. Check the installed Arduino tool layout and the platform upload recipe.' -f $Path)
    }
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

function Get-Uf2ProbeSummary {
    param(
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = ''
    )

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
            }

            $labelMatches = [string]::IsNullOrWhiteSpace($ExpectedLabel) -or [string]::IsNullOrWhiteSpace($summary.Label) -or $summary.Label -eq $ExpectedLabel
            $modelMatches = [string]::IsNullOrWhiteSpace($ExpectedModel) -or [string]::IsNullOrWhiteSpace($summary.Model) -or $summary.Model -eq $ExpectedModel
            $boardIdMatches = [string]::IsNullOrWhiteSpace($ExpectedBoardId) -or [string]::IsNullOrWhiteSpace($summary.BoardId) -or $summary.BoardId -eq $ExpectedBoardId
            if ($labelMatches -and $modelMatches -and $boardIdMatches) {
                return $summary
            }

            if ([string]::IsNullOrWhiteSpace($ExpectedLabel) -and [string]::IsNullOrWhiteSpace($ExpectedModel) -and [string]::IsNullOrWhiteSpace($ExpectedBoardId)) {
                return $summary
            }
        }
    }

    return $null
}

function Resolve-PythonLaunch {
    if (-not [string]::IsNullOrWhiteSpace($env:NIUS_UF2_PYTHON_EXE)) {
        $explicit = $env:NIUS_UF2_PYTHON_EXE.Trim().Trim('"')
        if (Test-Path -LiteralPath $explicit) {
            return [pscustomobject]@{
                Exe = (Resolve-Path -LiteralPath $explicit).Path
                PrefixArgs = @()
            }
        }
    }

    foreach ($cand in @(
        "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python310\python.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python39\python.exe",
        "$env:USERPROFILE\Anaconda3\python.exe",
        'C:\ProgramData\Anaconda3\python.exe'
    )) {
        if (Test-Path -LiteralPath $cand) {
            return [pscustomobject]@{
                Exe = $cand
                PrefixArgs = @()
            }
        }
    }

    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python -and $python.Source -notmatch '\\WindowsApps\\') {
        return [pscustomobject]@{
            Exe = $python.Source
            PrefixArgs = @()
        }
    }

    $python3 = Get-Command python3 -ErrorAction SilentlyContinue
    if ($python3 -and $python3.Source -notmatch '\\WindowsApps\\') {
        return [pscustomobject]@{
            Exe = $python3.Source
            PrefixArgs = @()
        }
    }

    # Keep `py -3` as a last fallback only. On this machine py.exe exists even
    # when no runnable Python 3 runtime is registered, which makes UF2 fallback
    # fail with the launcher message "No suitable Python runtime found".
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        $null = & $py.Source -3 --version 2>$null
        if ($LASTEXITCODE -eq 0) {
            return [pscustomobject]@{
                Exe = $py.Source
                PrefixArgs = @('-3')
            }
        }
    }

    throw 'Python 3 was not found. UF2 upload mode needs Python for build_uf2.py  -  install Python 3 from python.org, set NIUS_UF2_PYTHON_EXE to python.exe, or ensure `py -3` / `python` / `python3` works.'
}

function Wait-ForUsbBootloader {
    param(
        [string]$Exe,
        [string]$UsbVid,
        [string]$UsbPid,
        [string]$ExpectedLabel,
        [string]$ExpectedModel,
        [string]$ExpectedBoardId,
        [int]$Attempts = 20,
        [int]$DelayMs = 500
    )

    for ($attempt = 0; $attempt -lt $Attempts; $attempt++) {
        $output = & $Exe -l 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and $output -match [Regex]::Escape(('{0}:{1}' -f $UsbVid, $UsbPid).ToLower())) {
            return [pscustomobject]@{
                Kind = 'dfu'
                Summary = $null
            }
        }

        $uf2 = Get-Uf2ProbeSummary -ExpectedLabel $ExpectedLabel -ExpectedModel $ExpectedModel -ExpectedBoardId $ExpectedBoardId
        if ($uf2) {
            return [pscustomobject]@{
                Kind = 'uf2'
                Summary = $uf2
            }
        }

        Start-Sleep -Milliseconds $DelayMs
    }

    return $null
}

function Wait-ForUf2Volume {
    param(
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = '',
        [int]$Attempts = 40,
        [int]$DelayMs = 500
    )

    for ($attempt = 0; $attempt -lt $Attempts; $attempt++) {
        $uf2 = Get-Uf2ProbeSummary -ExpectedLabel $ExpectedLabel -ExpectedModel $ExpectedModel -ExpectedBoardId $ExpectedBoardId
        if ($uf2) {
            return $uf2
        }
        Start-Sleep -Milliseconds $DelayMs
    }

    return $null
}

# Known nRF52 bootloader USB identities, ordered by likelihood for the
# AliExpress / nice!nano-class clone fleet. Adafruit-fork UF2 PIDs come first
# because they are by far the most common on these boards; Nordic Open DFU
# PIDs follow as a fallback. Each entry carries the link-layout values
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
    @{ Vid = '239a'; Pid = '0029'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x26000'; MaxSize = 876544;  Note = 'Adafruit nice!nano v2 fork'; VolumeLabel = 'NICENANO'; Model = 'nice!nano'; BoardId = 'nRF52840-nicenano' },
    @{ Vid = '239a'; Pid = '002a'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x26000'; MaxSize = 876544;  Note = 'Adafruit Feather nRF52840 fork'; VolumeLabel = 'FTHR840BOOT'; Model = 'Feather nRF52840 Express'; BoardId = 'nRF52840-feather-express' },
    @{ Vid = '239a'; Pid = '4029'; Kind = 'adafruit-dfu'; Family = '0xADA52840'; AppStart = '0x26000'; MaxSize = 876544;  Note = 'Adafruit user-mode CDC (nRF52840)' },
    @{ Vid = '1915'; Pid = '521f'; Kind = 'dfu';          Family = '';           AppStart = '0x0';     MaxSize = 1032192; Note = 'Nordic Open DFU (legacy)' }
)
foreach ($pidByte in @('5280','5281','5282','5283','5284','5285','5286','5287','5288','5289','528a','528b','528c','528d','528e','528f')) {
    $script:NrfBootloaderCandidates += @{ Vid = '1915'; Pid = $pidByte; Kind = 'dfu'; Family = ''; AppStart = '0x0'; MaxSize = 1032192; Note = ('Nordic Open DFU clone (PID 0x{0})' -f $pidByte) }
}

function Find-PnpVidPid {
    param(
        [string]$VidLetters,
        [string]$PidLetters
    )

    $needle = ('VID_{0}&PID_{1}' -f $VidLetters, $PidLetters).ToUpperInvariant()
    $candidates = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue
    foreach ($device in $candidates) {
        $hwId = ($device.HardwareID -join ';').ToUpperInvariant()
        if ($hwId -match [Regex]::Escape($needle)) {
            return $device
        }
    }
    return $null
}

function Get-PnpVidPidMatches {
    param(
        [string]$VidLetters,
        [string]$PidLetters
    )

    $needle = ('USB\VID_{0}&PID_{1}' -f $VidLetters, $PidLetters).ToUpperInvariant()
    $candidates = @(Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue)
    return @($candidates | Where-Object {
        $instanceId = ([string]$_.InstanceId).ToUpperInvariant()
        $hardwareId = ([string](($_.HardwareID -join ';'))).ToUpperInvariant()
        $instanceId.StartsWith($needle) -or ($hardwareId -match [Regex]::Escape(('VID_{0}&PID_{1}' -f $VidLetters, $PidLetters).ToUpperInvariant()))
    })
}

function Format-PnpMatchSummary {
    param([object[]]$Matches)

    if (-not $Matches -or $Matches.Count -eq 0) {
        return '(no matching PnP nodes present)'
    }

    return (($Matches | ForEach-Object {
        $name = if ([string]::IsNullOrWhiteSpace($_.FriendlyName)) { $_.Class } else { $_.FriendlyName }
        ('{0} [{1}]' -f $name, $_.InstanceId)
    }) -join '; ')
}

function Get-AdafruitRuntimeSnapshot {
    param(
        [string]$BootloaderVid,
        [string]$BootloaderPid,
        [string]$RuntimeVid = '',
        [string]$RuntimePid = ''
    )

    $vidLetters = $BootloaderVid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $pidLetters = $BootloaderPid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $matches = @(Get-PnpVidPidMatches -VidLetters $vidLetters -PidLetters $pidLetters)
    # MI_02 is MSC in the bootloader but user CDC in our dual-CDC runtime.
    # Exclude Ports/Modem class devices so that user CDC on MI_02 is not
    # incorrectly counted as a storage (MSC/UF2) interface.
    $storageInterfaces = @($matches | Where-Object {
        ([string]$_.InstanceId).ToUpperInvariant() -like '*&MI_02*' -and
        ([string]$_.Class).ToUpperInvariant() -notin @('PORTS', 'MODEM')
    })
    $cdcInterfaces = @($matches | Where-Object { ([string]$_.InstanceId).ToUpperInvariant() -like '*&MI_00*' })
    $composites = @($matches | Where-Object { ([string]$_.InstanceId).ToUpperInvariant() -notlike '*&MI_*' })

    $runtimePresent = $false
    $runtimeSummary = 'runtime=untracked'
    if (-not [string]::IsNullOrWhiteSpace($RuntimeVid) -and -not [string]::IsNullOrWhiteSpace($RuntimePid)) {
        $runtimeVidLetters = $RuntimeVid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
        $runtimePidLetters = $RuntimePid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
        $runtimeMatches = @(Get-PnpVidPidMatches -VidLetters $runtimeVidLetters -PidLetters $runtimePidLetters)
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

function Wait-ForAdafruitRuntimeTransition {
    param(
        [string]$BootloaderVid,
        [string]$BootloaderPid,
        [string]$RuntimeVid = '',
        [string]$RuntimePid = '',
        [int]$Attempts = 240,
        [int]$DelayMs = 500
    )

    $stableRuntimeDetections = 0
    $lastSnapshot = $null
    for ($attempt = 0; $attempt -lt $Attempts; $attempt++) {
        $lastSnapshot = Get-AdafruitRuntimeSnapshot -BootloaderVid $BootloaderVid -BootloaderPid $BootloaderPid -RuntimeVid $RuntimeVid -RuntimePid $RuntimePid
        $runtimeSatisfied = [string]::IsNullOrWhiteSpace($RuntimeVid) -or [string]::IsNullOrWhiteSpace($RuntimePid) -or $lastSnapshot.RuntimePresent
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
        [string]$DfuToolPath,
        [int]$Attempts = 4,
        [int]$DelayMs = 400
    )

    $probedNames = ($script:NrfBootloaderCandidates | ForEach-Object { ('{0}:{1}' -f $_.Vid, $_.Pid) }) -join ', '

    for ($attempt = 0; $attempt -lt $Attempts; $attempt++) {
        # 1) Ask dfu-util what it sees. Cheap and authoritative for Nordic DFU.
        $dfuOutput = ''
        try {
            $dfuOutput = & $DfuToolPath -l 2>&1 | Out-String
        } catch {
            $dfuOutput = ''
        }
        $dfuLower = if ([string]::IsNullOrWhiteSpace($dfuOutput)) { '' } else { $dfuOutput.ToLowerInvariant() }

        foreach ($candidate in $script:NrfBootloaderCandidates) {
            $needle = ('{0}:{1}' -f $candidate.Vid, $candidate.Pid)
            $hitDfu = ($dfuLower -ne '' -and $dfuLower -match [Regex]::Escape($needle))
            $hitPnp = (Find-PnpVidPid -VidLetters $candidate.Vid -PidLetters $candidate.Pid) -ne $null
            if ($hitDfu -or $hitPnp) {
                $expectedLabel = if ($candidate.ContainsKey('VolumeLabel')) { $candidate.VolumeLabel } else { '' }
                $expectedModel = if ($candidate.ContainsKey('Model')) { $candidate.Model } else { '' }
                $expectedBoardId = if ($candidate.ContainsKey('BoardId')) { $candidate.BoardId } else { '' }
                $uf2 = $null
                if (-not [string]::IsNullOrWhiteSpace($candidate.Family) -or $candidate.Kind -eq 'uf2') {
                    $uf2 = Get-Uf2ProbeSummary -ExpectedLabel $expectedLabel -ExpectedModel $expectedModel -ExpectedBoardId $expectedBoardId
                }
                $resolvedKind = if ($uf2) { 'uf2' } else { $candidate.Kind }
                Write-NiusDetail ('[nius] auto-detect: matched {0} via {1} -- {2}{3}' -f $needle, $(if ($hitDfu) { 'dfu-util' } else { 'PnP' }), $candidate.Note, $(if ($uf2) { '; mounted UF2 volume preferred' } else { '' }))
                return [pscustomobject]@{
                    Resolved = $true
                    Source = $(if ($hitDfu) { 'dfu' } else { 'pnp' })
                    Vid = ('0x{0}' -f $candidate.Vid)
                    Pid = ('0x{0}' -f $candidate.Pid)
                    Kind = $resolvedKind
                    Family = $candidate.Family
                    AppStart = $candidate.AppStart
                    MaxSize = $candidate.MaxSize
                    Note = $candidate.Note
                    VolumeLabel = if ($uf2) { $uf2.Label } elseif ($candidate.ContainsKey('VolumeLabel')) { $candidate.VolumeLabel } else { '' }
                    Model = if ($uf2) { $uf2.Model } elseif ($candidate.ContainsKey('Model')) { $candidate.Model } else { '' }
                    BoardId = if ($uf2) { $uf2.BoardId } elseif ($candidate.ContainsKey('BoardId')) { $candidate.BoardId } else { '' }
                    DriveRoot = if ($uf2) { $uf2.Drive } else { '' }
                }
            }
        }

        # 2) Last-resort: any UF2 volume whose INFO_UF2.TXT looks like an
        # nRF52840 bootloader, even if VID/PID didn't match a known clone.
        # (We do prefer 'adafruit-dfu' for VID/PID-matched Adafruit forks;
        # this fallback uses the UF2 mass-storage path because we got here
        # via a mounted volume rather than a USB identity match.)
        $uf2Any = Get-Uf2ProbeSummary
        if ($uf2Any) {
            Write-NiusDetail ('[nius] auto-detect: matched UF2 volume {0} (label={1}, model={2}); VID/PID unknown, treating as Adafruit-fork UF2' -f $uf2Any.Drive, $uf2Any.Label, $uf2Any.Model)
            return [pscustomobject]@{
                Resolved = $true
                Source = 'uf2-volume'
                Vid = '0x239A'
                Pid = '0x00B3'
                Kind = 'uf2'
                Family = '0xADA52840'
                AppStart = '0x26000'
                MaxSize = 876544
                Note = 'UF2 volume identified by INFO_UF2.TXT only'
                VolumeLabel = $uf2Any.Label
                Model = $uf2Any.Model
                BoardId = $uf2Any.BoardId
                DriveRoot = $uf2Any.Drive
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
        [string]$DrivePath
    )

    $uf2Path = New-Uf2Artifact -HexPath $HexPath -FamilyId $FamilyId
    $destination = Join-Path $DrivePath ([System.IO.Path]::GetFileName($uf2Path))
    Copy-Item -LiteralPath $uf2Path -Destination $destination -Force
}

function New-Uf2Artifact {
    param(
        [string]$HexPath,
        [string]$FamilyId
    )

    if ([string]::IsNullOrWhiteSpace($HexPath) -or -not (Test-Path -LiteralPath $HexPath)) {
        throw ('Compiled hex file not found for UF2 upload: {0}' -f $HexPath)
    }

    if ([string]::IsNullOrWhiteSpace($FamilyId)) {
        throw 'UF2 upload was selected, but no UF2 family ID was provided by the board recipe.'
    }

    $python = Resolve-PythonLaunch
    $converter = Join-Path $PSScriptRoot 'build_uf2.py'
    Assert-ToolExists -Path $converter
    $uf2Path = [System.IO.Path]::ChangeExtension($HexPath, '.uf2')
    $args = @()
    $args += $python.PrefixArgs
    $args += @($converter, '--input-hex', $HexPath, '--output-uf2', $uf2Path, '--family-id', $FamilyId)
    Invoke-CommandChecked -Exe $python.Exe -Arguments $args -FailureKind 'uf2-convert'
    return $uf2Path
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

    $allowConda = ($env:NIUS_ALLOW_ANACONDA_ADAFRUIT_NRFUTIL -eq '1')

    # Prefer the first non-conda hit.
    foreach ($p in $ordered) {
        if (-not (Test-NiusResolvedPathUnderConda $p)) {
            return $p
        }
    }

    # Only conda installs available.
    if ($allowConda) {
        Write-NiusDetail '[nius] NIUS_ALLOW_ANACONDA_ADAFRUIT_NRFUTIL=1: using a Conda adafruit-nrfutil (nordicsemi version may mismatch).' -ForegroundColor Yellow
        return $ordered[0]
    }
    Write-NiusDetail '[nius] Only a Conda adafruit-nrfutil was found; serial DFU may fail on a version mismatch. Prefer `pip install adafruit-nrfutil` with python.org Python, or set NIUS_ADAFRUIT_NRFUTIL_EXE.' -ForegroundColor Yellow
    return $ordered[0]
}

function Stop-NiusLingeringAdafruitNrfutil {
    param(
        [ValidateSet('touch', 'dfu')]
        [string]$Phase = 'touch'
    )

    if ($Phase -eq 'touch' -and $env:NIUS_SKIP_STOP_NRFUTIL_BEFORE_TOUCH -eq '1') {
        return
    }

    if ($Phase -eq 'dfu' -and $env:NIUS_SKIP_STOP_NRFUTIL_BEFORE_DFU -eq '1') {
        return
    }

    # Back-to-back uploads on Windows: leftover nrfutil or PYTHON.EXE child often keeps COM exclusive.
    cmd /c "taskkill /F /IM adafruit-nrfutil.exe 2>nul" | Out-Null
    Start-Sleep -Milliseconds 200
    cmd /c "taskkill /F /IM adafruit-nrfutil.exe 2>nul" | Out-Null
    $settleMs = if ($Phase -eq 'dfu') { 450 } else { 380 }
    Start-Sleep -Milliseconds $settleMs
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
        throw 'adafruit-nrfutil was not found. Install with non-Conda Python (`pip install adafruit-nrfutil`), set NIUS_ADAFRUIT_NRFUTIL_EXE to the Scripts\adafruit-nrfutil.exe path, or temporarily NIUS_ALLOW_ANACONDA_ADAFRUIT_NRFUTIL=1 if you must use Conda. UF2 drag-drop is also supported via the UF2 bootloader menu entries.'
    }
    Write-NiusDetail ('[nius] Resolved adafruit-nrfutil: {0}' -f $tool) -ForegroundColor DarkGray

    Write-NiusDetail '[nius] Adafruit DFU pipeline starting (genpkg, then serial)...' -ForegroundColor DarkGray

    Stop-NiusLingeringAdafruitNrfutil -Phase dfu

    $zipPath = [System.IO.Path]::ChangeExtension($HexPath, '.zip')

    $resolvedSdReq = $SdReq
    if ([string]::IsNullOrWhiteSpace($resolvedSdReq) -and $DevType -eq '0x0052') {
        # Adafruit's nRF52840 Arduino BSP packages serial DFU updates with
        # S140 6.1.1 FWID 0x00B6. Leaving adafruit-nrfutil at its default
        # 0xFFFE metadata diverges from the mature reference path and can make
        # clone bootloaders accept the transfer but refuse to launch the app.
        $resolvedSdReq = '0x00B6'
    }

    $genpkgArgs = @('dfu', 'genpkg', '--dev-type', $DevType)
    if (-not [string]::IsNullOrWhiteSpace($resolvedSdReq)) {
        $genpkgArgs += @('--sd-req', $resolvedSdReq)
    }
    $genpkgArgs += @('--application', $HexPath, $zipPath)

    if ($script:NiusVerbose) { Write-Stage -Percent 55 -Label 'Generating Adafruit DFU package (genpkg)' }
    Invoke-CommandChecked -Exe $tool -Arguments $genpkgArgs -FailureKind 'adafruit-genpkg' -ProgressPercent 60 -ProgressLabel 'genpkg synthesizing DFU package'

    if (-not (Test-Path -LiteralPath $zipPath)) {
        throw ('adafruit-nrfutil genpkg did not produce expected output: {0}' -f $zipPath)
    }

    try {
        $serialReadyMs = 12000
        if (-not [string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS)) {
            $sr = -1
            if ([int]::TryParse($env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS, [ref]$sr) -and $sr -gt 0) {
                $serialReadyMs = $sr
            }
        }
        Wait-SerialPortReady -PortName $Port -Purpose 'Adafruit DFU control port' -TimeoutMs $serialReadyMs
    } catch {
        Throw-NiusUploadFailure (New-UploadFailure -Kind 'adafruit-dfu' -ExitCode 1 -Output $_.Exception.Message -Exe $tool)
    }

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
    Invoke-CommandChecked -Exe $tool -Arguments @('--verbose', 'dfu', 'serial', '-pkg', $zipPath, '-p', $Port, '-b', [string]$BaudRate, '-sb') -FailureKind 'adafruit-dfu' -ProgressPercent 90 -ProgressLabel 'Uploading' -TotalFrames $totalFrames -FirmwareBytes $fwBytes
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
            if ([int]::TryParse($override, [ref]$parsedTimeout)) {
                if ($parsedTimeout -eq 0) {
                    $configuredTimeoutMs = 0
                }
                elseif ($parsedTimeout -gt 0) {
                    $configuredTimeoutMs = $parsedTimeout
                }
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
                if ([int]::TryParse($idleOverride, [ref]$parsedIdle)) {
                    if ($parsedIdle -eq 0) {
                        $idleMs = 0
                    }
                    elseif ($parsedIdle -gt 0) {
                        $idleMs = $parsedIdle
                    }
                }
            }
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
                LastUtc = [datetime]::UtcNow
                IdleResetOnAnyLine = [bool]$idleResetOnAnyLine
                Hashes = 0
            })

        $idleSrcOut = 'nius-adf-out-' + ([Guid]::NewGuid().ToString('n'))
        $idleSrcErr = 'nius-adf-err-' + ([Guid]::NewGuid().ToString('n'))

        $null = Register-ObjectEvent -InputObject $process -EventName OutputDataReceived -SourceIdentifier $idleSrcOut -MessageData $streamSync -Action {
            $ea = $EventArgs
            if ($null -ne $ea.Data -and $ea.Data.Length -gt 0) {
                $sync = $Event.MessageData
                [void]$sync.Lines.Add($ea.Data)
                $trimmed = $ea.Data.TrimStart()
                # adafruit-nrfutil echoes one '#' per 512 B DFU frame, newline
                # every 40 frames. Count pure-'#' lines to drive the byte bar.
                $hashOnly = $ea.Data.Trim()
                if ($hashOnly.Length -gt 0 -and $hashOnly -match '^#+$') {
                    $sync.Hashes += $hashOnly.Length
                }
                if ($sync.IdleResetOnAnyLine -or ($trimmed -notmatch '^(?i)(DEBUG|INFO)\s')) {
                    $sync.LastUtc = [datetime]::UtcNow
                }
            }
        }

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

        $process.BeginOutputReadLine()
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
                    # Real byte-based bar: map streamed frames onto 0..98% and
                    # only emit a line when we cross a new 20% band, so the IDE
                    # console shows ~5-6 clean lines instead of a flood.
                    $done = [int]$streamSync.Hashes
                    $frac = if ($TotalFrames -gt 0) { $done / [double]$TotalFrames } else { 0 }
                    if ($frac -lt 0) { $frac = 0 }
                    if ($frac -gt 1) { $frac = 1 }
                    $pct = [int][Math]::Floor($frac * 100)
                    if ($pct -gt 98) { $pct = 98 }
                    $band = [int][Math]::Floor($pct / 20)
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
            Start-Sleep -Milliseconds 120
        }

        if (-not $timedOutIdle) {
            $null = $process.WaitForExit(0)
        }

        $output = (@($streamSync.Lines) -join [Environment]::NewLine).Trim()
    }
    else {
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

        $stdout = $process.StandardOutput.ReadToEnd()
        $stderr = $process.StandardError.ReadToEnd()
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
        $label = if ($FailureKind -eq 'adafruit-genpkg') { 'adafruit-nrfutil genpkg' } else { 'adafruit-nrfutil serial DFU' }
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

    if ($process.ExitCode -ne 0 -or $textSignaledFailure) {
        $reportedExitCode = if ($process.ExitCode -ne 0) { $process.ExitCode } else { 1 }
        Throw-NiusUploadFailure (New-UploadFailure -Kind $FailureKind -ExitCode $reportedExitCode -Output $output -Exe $Exe)
    }
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
                    'Optional: NIUS_ALLOW_USER_CDC_UPLOAD_PORT=1 restores automatic port remap.'
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

function Test-NiusAdafruitDfuFailureIsTimeout {
    param([object]$Failure)

    if (-not $Failure) {
        return $false
    }

    if ($Failure.Kind -ne 'adafruit-dfu' -and $Failure.Kind -ne 'adafruit-genpkg') {
        return $false
    }

    $blob = ('{0} {1}' -f $Failure.Summary, (($Failure.Details | ForEach-Object { $_ }) -join ' ')).ToLowerInvariant()
    return $blob -match 'stalled: no new|stalled: no stdout/stderr lines|serial dfu stalled|serial dfu timed out|no response from device|timed out waiting for bootloader response|exceeded process timeout|exit=124|exit=125'
}

function Test-NiusAdafruitDfuFailureNeedsAutoRetouch {
    param([object]$Failure)

    if ($env:NIUS_SKIP_AUTO_DFU_RETOUCH_RETRY -eq '1') {
        return $false
    }

    if (-not $Failure) {
        return $true
    }

    if ($Failure.Kind -ne 'adafruit-dfu' -and $Failure.Kind -ne 'adafruit-genpkg') {
        return $false
    }

    if (Test-NiusAdafruitDfuFailureIsTimeout -Failure $Failure) {
        return $false
    }

    $blob = ('{0} {1}' -f $Failure.Summary, (($Failure.Details | ForEach-Object { $_ }) -join ' ')).ToLowerInvariant()
    if ($blob -match 'wrong com for adafruit serial dfu|user cdc serial cannot') {
        return $false
    }

    return $blob -match 'could not open port|access is denied|permission denied|sharing violation|being used|busy|failed to upgrade|adafruit serial dfu command failed|exit code 1'
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

    if ($Kind -eq 'dfu-wait') {
        $uf2 = Get-Uf2ProbeSummary
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

    return ('Command failed with exit code {0}: {1}' -f $formattedExitCode, $Exe)
}

function Touch-SerialPort1200 {
    param(
        [string]$PortName,
        [bool]$IncludePrepulse115200 = $false
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        return $false
    }

    function Invoke-TouchPulse {
        param([int]$BaudRate)

        $openTimeoutSec = 14
        $envMs = $env:NIUS_TOUCH_SERIAL_OPEN_TIMEOUT_MS
        if (-not [string]::IsNullOrWhiteSpace($envMs)) {
            $parsedOpen = -1
            if ([int]::TryParse($envMs, [ref]$parsedOpen) -and $parsedOpen -gt 0) {
                $openTimeoutSec = [int][Math]::Ceiling($parsedOpen / 1000.0)
                if ($openTimeoutSec -lt 1) {
                    $openTimeoutSec = 1
                }
            }
        }

        # SerialPort.Open() can wedge indefinitely on Windows; run in a child job so we can stop it on timeout.
        $touchSb = {
            param([string]$pn, [int]$br)
            $serial = $null
            try {
                $serial = New-Object System.IO.Ports.SerialPort $pn, $br, 'None', 8, 'One'
                $serial.DtrEnable = $true
                $serial.RtsEnable = $true
                $serial.ReadTimeout = 250
                $serial.WriteTimeout = 250
                $serial.Open()
                Start-Sleep -Milliseconds 120
                # Core triggers bootloader prep when SERVICE CDC line coding is 1200 *and* host drops DTR
                # (CDC_REQ_SET_CONTROL_LINE_STATE: previousDtr && !dtr). Dispose alone can miss the edge on some hosts.
                if ($br -eq 1200) {
                    Start-Sleep -Milliseconds 80
                    $serial.DtrEnable = $false
                    Start-Sleep -Milliseconds 100
                }
                return $true
            }
            catch {
                return $false
            }
            finally {
                if ($null -ne $armSerial) {
                    try {
                        $armSerial.Dispose()
                    }
                    catch {
                    }
                }
                if ($null -ne $serial) {
                    try {
                        $serial.Dispose()
                    }
                    catch {
                    }
                }
            }
        }

        $job = Start-Job -ScriptBlock $touchSb -ArgumentList $PortName, $BaudRate
        $completed = Wait-Job -Job $job -Timeout $openTimeoutSec
        if (-not $completed) {
            try {
                Stop-Job -Job $job -Force -ErrorAction SilentlyContinue
            }
            catch {
            }
            try {
                Remove-Job -Job $job -Force -ErrorAction SilentlyContinue
            }
            catch {
            }
            return $false
        }

        $recv = $null
        try {
            $recv = Receive-Job -Job $job -ErrorAction SilentlyContinue
        }
        catch {
            $recv = $false
        }
        finally {
            try {
                Remove-Job -Job $job -Force -ErrorAction SilentlyContinue
            }
            catch {
            }
        }

        return [bool]$recv
    }

    Write-NiusDetail ('[nius] 1200bps touch on {0} (~28s budget; Open timeout via NIUS_TOUCH_SERIAL_OPEN_TIMEOUT_MS)...' -f $PortName) -ForegroundColor DarkGray

    $pulseLogPhase = ''
    $pulseLogUtc = [datetime]::UtcNow.AddMinutes(-5)

    $deadline = (Get-Date).AddMilliseconds(28000)
    $lastError = ''
    $lastHeartbeat = Get-Date
    while ((Get-Date) -lt $deadline) {
        if (((Get-Date) - $lastHeartbeat).TotalMilliseconds -ge 2000) {
            $remainSec = [int][Math]::Max(0, [Math]::Ceiling(($deadline - (Get-Date)).TotalSeconds))
            Write-NiusDetail ('[nius]   touch still running on {0} (~{1}s left)...' -f $PortName, $remainSec) -ForegroundColor DarkGray
            $lastHeartbeat = Get-Date
        }

        try {
            if ($IncludePrepulse115200) {
                $utcPulse = [datetime]::UtcNow
                $phase115200Changed = ($pulseLogPhase -ne '115200')
                $pulseElapsedMs = ($utcPulse - $pulseLogUtc).TotalMilliseconds
                if ($phase115200Changed -or $pulseElapsedMs -ge 3500) {
                    Write-NiusDetail ('[nius]   touch pulse 115200 on {0}...' -f $PortName) -ForegroundColor DarkGray
                    $pulseLogPhase = '115200'
                    $pulseLogUtc = $utcPulse
                }
                if (-not (Invoke-TouchPulse -BaudRate 115200)) {
                    throw '115200 phase failed'
                }
                Start-Sleep -Milliseconds 200
            }

            $utcPulse = [datetime]::UtcNow
            $phase1200Changed = ($pulseLogPhase -ne '1200')
            $pulseElapsedMs = ($utcPulse - $pulseLogUtc).TotalMilliseconds
            if ($phase1200Changed -or $pulseElapsedMs -ge 3500) {
                Write-NiusDetail ('[nius]   touch pulse 1200 on {0}...' -f $PortName) -ForegroundColor DarkGray
                $pulseLogPhase = '1200'
                $pulseLogUtc = $utcPulse
            }
            if (-not (Invoke-TouchPulse -BaudRate 1200)) {
                throw '1200 phase failed'
            }
            Start-Sleep -Milliseconds 200
            return $true
        }
        catch {
            $lastError = $_.Exception.Message
            Start-Sleep -Milliseconds 450
        }
    }

    Write-NiusDetail ('[warn] 1200bps touch skipped on {0}: {1}' -f $PortName, $lastError)
    return $false
}

function Invoke-Touch1200Transition {
    param(
        [string]$PortName,
        [string]$BootloaderVid = '',
        [string]$BootloaderPid = '',
        [string]$RuntimeVid = '',
        [string]$RuntimePid = '',
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = ''
    )

    $touchModes = @(
        [pscustomobject]@{
            Label = 'legacy 115200->1200 touch'
            IncludePrepulse115200 = $true
        },
        [pscustomobject]@{
            Label = 'single 1200 touch'
            IncludePrepulse115200 = $false
        }
    )

    foreach ($touchMode in $touchModes) {
        if (-not (Touch-SerialPort1200 -PortName $PortName -IncludePrepulse115200:$touchMode.IncludePrepulse115200)) {
            continue
        }

        try {
            if (-not [string]::IsNullOrWhiteSpace($BootloaderVid) -and -not [string]::IsNullOrWhiteSpace($BootloaderPid)) {
                Wait-SamePidAdafruitBootloaderTransition `
                    -PortName $PortName `
                    -BootloaderVid $BootloaderVid `
                    -BootloaderPid $BootloaderPid `
                    -RuntimeVid $RuntimeVid `
                    -RuntimePid $RuntimePid `
                    -ExpectedLabel $ExpectedLabel `
                    -ExpectedModel $ExpectedModel `
                    -ExpectedBoardId $ExpectedBoardId `
                    -Purpose $touchMode.Label | Out-Null
            }
            else {
                Wait-SerialPortResetCycle -PortName $PortName -Purpose $touchMode.Label
            }

            return [pscustomobject]@{
                Triggered = $true
                Candidate = $touchMode
            }
        }
        catch {
            Write-NiusDetail ('[warn] {0} on {1} did not produce a confirmed bootloader transition: {2}' -f $touchMode.Label, $PortName, $_.Exception.Message) -ForegroundColor DarkYellow
        }
    }

    return [pscustomobject]@{
        Triggered = $false
        Candidate = $null
    }
}

function Get-NiusPostTouchSleepMilliseconds {
    # Buttonless boards: give USB detach / bootloader enumerate before nrfutil grabs COM.
    # NOTE: this is now used as the *ceiling* of the adaptive settle wait
    # (Wait-NiusBootloaderPortSettled), not a blind fixed sleep, so a fast
    # board no longer pays the full window. Set NIUS_FORCE_FIXED_POST_TOUCH_SLEEP=1
    # to fall back to a blind Start-Sleep of this duration.
    $ms = 3800
    $o = $env:NIUS_POST_TOUCH_SLEEP_MS
    if (-not [string]::IsNullOrWhiteSpace($o)) {
        $p = -1
        if ([int]::TryParse($o, [ref]$p) -and $p -ge 0) {
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

function Invoke-NiusAdafruitDfuRetouchWait {
    param(
        [string]$PortName,
        [string]$Reason,
        [string]$BootloaderVid = '',
        [string]$BootloaderPid = '',
        [string]$RuntimeVid = '',
        [string]$RuntimePid = '',
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = ''
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        return
    }

    Stop-NiusLingeringAdafruitNrfutil -Phase touch

    Write-NiusDetail ('[nius] {0}' -f $Reason)
    $touchTransition = Invoke-Touch1200Transition `
        -PortName $PortName `
        -BootloaderVid $BootloaderVid `
        -BootloaderPid $BootloaderPid `
        -RuntimeVid $RuntimeVid `
        -RuntimePid $RuntimePid `
        -ExpectedLabel $ExpectedLabel `
        -ExpectedModel $ExpectedModel `
        -ExpectedBoardId $ExpectedBoardId
    if (-not $touchTransition.Triggered) {
        Write-NiusDetail ('[warn] Re-touch failed on {0}; continuing in case the port is already in bootloader.' -f $PortName)
        return $false
    }
    return $true
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
    $count = @(Get-SerialPortInventory | Where-Object {
        ([string]$_.DeviceID).Trim().ToUpperInvariant() -eq $normalizedPort
    }).Count
    return $count -gt 0
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
    $portRecord = @(Get-SerialPortInventory | Where-Object {
        ([string]$_.DeviceID).Trim().ToUpperInvariant() -eq $normalizedPort
    } | Select-Object -First 1)
    if (-not $portRecord) {
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

function Wait-SamePidAdafruitBootloaderTransition {
    param(
        [string]$PortName,
        [string]$BootloaderVid,
        [string]$BootloaderPid,
        [string]$RuntimeVid = '',
        [string]$RuntimePid = '',
        [string]$ExpectedLabel = '',
        [string]$ExpectedModel = '',
        [string]$ExpectedBoardId = '',
        [int]$TimeoutMs = 12000,
        [string]$Purpose = 'same-PID bootloader transition'
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

    while ((Get-Date) -lt $deadline) {
        # PnP-only: do NOT open the port - opening at 115200 baud would send
        # SET_LINE_CODING(115200) to the firmware and cancel the pending 1200bps touch.
        $portPresent = Test-SerialPortPnpPresent -PortName $PortName
        if (-not $portPresent) {
            $sawDetach = $true
            $lastIssue = 'port detached'
        }

        $lastSnapshot = Get-AdafruitRuntimeSnapshot -BootloaderVid $BootloaderVid -BootloaderPid $BootloaderPid -RuntimeVid $RuntimeVid -RuntimePid $RuntimePid
        $uf2Probe = $null
        try {
            $uf2Probe = Get-Uf2ProbeSummary -ExpectedLabel $ExpectedLabel -ExpectedModel $ExpectedModel -ExpectedBoardId $ExpectedBoardId
        }
        catch {
            $uf2Probe = $null
        }

        $strongBootloaderEvidence = ($lastSnapshot.StorageInterfaceCount -gt 0) -or ($null -ne $uf2Probe)
        if ($strongBootloaderEvidence -and $portPresent) {
            return [pscustomobject]@{
                Mode = 'bootloader-evidence'
                Summary = $lastSnapshot.Summary
                Uf2 = $uf2Probe
            }
        }

        if ($sawDetach -and $portPresent) {
            return [pscustomobject]@{
                Mode = 'reset-cycle'
                Summary = if ($lastSnapshot) { $lastSnapshot.Summary } else { '' }
                Uf2 = $uf2Probe
            }
        }

        if ((Get-Date) -ge $nextProgressAt) {
            $elapsed = [int](((Get-Date) - $waitStarted).TotalMilliseconds)
            $summary = if ($lastSnapshot) { $lastSnapshot.Summary } else { 'snapshot unavailable' }
            Write-NiusDetail ('[nius]   same-PID wait {0} ms / {1} ms - last: {2}; snapshot: {3}' -f $elapsed, $TimeoutMs, $lastIssue, $summary) -ForegroundColor DarkGray
            $nextProgressAt = (Get-Date).AddMilliseconds(2500)
        }

        Start-Sleep -Milliseconds 220
    }

    $summary = if ($lastSnapshot) { $lastSnapshot.Summary } else { 'snapshot unavailable' }
    throw ('No same-PID bootloader transition observed after touch ({0}); snapshot: {1}' -f $lastIssue, $summary)
}

$toolPath = [System.IO.Path]::GetFullPath($Tool)
try {
    Assert-ToolExists -Path $toolPath
    Write-Banner -BoardName $Board
    Write-Section -Label 'transport handshake initialized'
    $expectedRuntimeIdentity = Resolve-ExpectedRuntimeUsbIdentity -BoardName $Board
    $effectiveRuntimeUsbVid = if ($expectedRuntimeIdentity) { [string]$expectedRuntimeIdentity.Vid } else { '' }
    $effectiveRuntimeUsbPid = if ($expectedRuntimeIdentity) { [string]$expectedRuntimeIdentity.Pid } else { '' }
    if (-not [string]::IsNullOrWhiteSpace($RuntimeUsbPid)) {
        $effectiveRuntimeUsbPid = $RuntimeUsbPid.Trim()
        if ([string]::IsNullOrWhiteSpace($effectiveRuntimeUsbVid) -and -not [string]::IsNullOrWhiteSpace($UsbVid) -and $UsbVid -ne 'auto') {
            $effectiveRuntimeUsbVid = $UsbVid.Trim()
        }
        $expectedRuntimeIdentity = [pscustomobject]@{
            Vid = $effectiveRuntimeUsbVid
            Pid = $effectiveRuntimeUsbPid
        }
    }
    $adafruitControlPort = $Port
    $controlPortAlreadyBootloader = $false
    if (-not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbVid) -and -not [string]::IsNullOrWhiteSpace($effectiveRuntimeUsbPid)) {
        $portResolution = Resolve-AdafruitSerialControlPort -SelectedPort $Port -RuntimeVid $effectiveRuntimeUsbVid -RuntimePid $effectiveRuntimeUsbPid
        if (-not $portResolution) {
            $portResolution = Resolve-AdafruitSerialControlPortWithBoardIdentity -SelectedPort $Port -BoardName $Board
        }
        if ($portResolution -and -not [string]::IsNullOrWhiteSpace($portResolution.Port)) {
            $remapWouldChange = ($portResolution.Port.Trim().ToUpperInvariant() -ne $Port.Trim().ToUpperInvariant())
            $strictRejectUserCdc = ($env:NIUS_ALLOW_USER_CDC_UPLOAD_PORT -ne '1')
            if ($BootloaderMode -eq 'adafruit-dfu' -and $remapWouldChange -and $strictRejectUserCdc) {
                $svc = $portResolution.Port
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'adafruit-dfu' -ExitCode 1 -Output (@(
                        'Wrong COM for Adafruit serial DFU: selected port is the Arduino USER CDC (Serial.print) interface, not the SERVICE / maintenance CDC used for 1200bps touch and nrfutil.',
                        ('Selected={0}; use SERVICE CDC instead (typically smaller COM index / MI_00): {1}.' -f $Port, $svc),
                        ('ZH: USER CDC serial cannot flash firmware; switch IDE Tools->Port to SERVICE CDC (MI_00): {0}' -f $svc),
                        'Optional legacy auto-remap: set NIUS_ALLOW_USER_CDC_UPLOAD_PORT=1 before upload.'
                    ) -join ' ') -Exe $toolPath)
            }
            $adafruitControlPort = $portResolution.Port
            if ($adafruitControlPort -ne $Port -and $portResolution -and ($env:NIUS_ALLOW_USER_CDC_UPLOAD_PORT -eq '1')) {
                Write-NiusDetail ('[nius] serial DFU control port remap: selected {0}, using {1} ({2})' -f $Port, $adafruitControlPort, $portResolution.Reason)
            }
        }
    }
    elseif ($expectedRuntimeIdentity) {
        $portResolution = Resolve-AdafruitSerialControlPortWithBoardIdentity -SelectedPort $Port -BoardName $Board
        if ($portResolution -and -not [string]::IsNullOrWhiteSpace($portResolution.Port)) {
            $remapWouldChange = ($portResolution.Port.Trim().ToUpperInvariant() -ne $Port.Trim().ToUpperInvariant())
            $strictRejectUserCdc = ($env:NIUS_ALLOW_USER_CDC_UPLOAD_PORT -ne '1')
            if ($BootloaderMode -eq 'adafruit-dfu' -and $remapWouldChange -and $strictRejectUserCdc) {
                $svc = $portResolution.Port
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'adafruit-dfu' -ExitCode 1 -Output (@(
                        'Wrong COM for Adafruit serial DFU: selected port is the Arduino USER CDC (Serial.print) interface, not the SERVICE / maintenance CDC used for 1200bps touch and nrfutil.',
                        ('Selected={0}; use SERVICE CDC instead (typically smaller COM index / MI_00): {1}.' -f $Port, $svc),
                        ('ZH: USER CDC serial cannot flash firmware; switch IDE Tools->Port to SERVICE CDC (MI_00): {0}' -f $svc),
                        'Optional legacy auto-remap: set NIUS_ALLOW_USER_CDC_UPLOAD_PORT=1 before upload.'
                    ) -join ' ') -Exe $toolPath)
            }
            $adafruitControlPort = $portResolution.Port
            if ($adafruitControlPort -ne $Port -and $portResolution -and ($env:NIUS_ALLOW_USER_CDC_UPLOAD_PORT -eq '1')) {
                Write-NiusDetail ('[nius] serial DFU control port remap: selected {0}, using {1} ({2})' -f $Port, $adafruitControlPort, $portResolution.Reason)
            }
        }
    }
    $runtimeSharesUploadIdentity = $false
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
            $samePidSnapshot = Get-AdafruitRuntimeSnapshot -BootloaderVid $UsbVid -BootloaderPid $UsbPid -RuntimeVid $effectiveRuntimeUsbVid -RuntimePid $effectiveRuntimeUsbPid
            $uf2Probe = $null
            try {
                $uf2Probe = Get-Uf2ProbeSummary -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId
            }
            catch {
                $uf2Probe = $null
            }
            $strongBootloaderEvidence = ($samePidSnapshot.StorageInterfaceCount -gt 0) -or ($null -ne $uf2Probe)
            if ($strongBootloaderEvidence) {
                Write-NiusDetail ('[nius] Same-PID upload path: bootloader evidence present on {0}; skipping 1200 touch ({1})' -f $adafruitControlPort, $samePidSnapshot.Summary) -ForegroundColor DarkGray
            }
            else {
                $controlPortAlreadyBootloader = $false
                Write-NiusDetail ('[nius] Same-PID upload path: no MSC/UF2 bootloader evidence on {0}; treating it as runtime service CDC and keeping 1200 touch enabled.' -f $adafruitControlPort) -ForegroundColor DarkGray
            }
        }
        if ($env:NIUS_ASSUME_SELECTED_PORT_BOOTLOADER -eq '1') {
            $controlPortAlreadyBootloader = $true
            $runtimeSharesUploadIdentity = $false
            Write-NiusDetail ('[nius] NIUS_ASSUME_SELECTED_PORT_BOOTLOADER=1 - treating {0} as an already-running bootloader port.' -f $adafruitControlPort) -ForegroundColor DarkGray
        }
    }

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
            $resolved = Resolve-AutoBootloader -DfuToolPath $toolPath -Attempts 2 -DelayMs 200
            if (-not $resolved.Resolved -and $UseTouch1200 -eq 'true') {
                Stop-NiusLingeringAdafruitNrfutil -Phase touch
                Write-NiusDetail '[nius] Entering bootloader (1200 bps touch)...' -ForegroundColor DarkGray
                if (-not (Touch-SerialPort1200 -PortName $adafruitControlPort)) {
                    Throw-NiusUploadFailure (New-UploadFailure -Kind 'adafruit-dfu' -ExitCode 1 -Output ('Unable to trigger 1200 bps touch on {0}; the service/user CDC port may be missing or busy.' -f $adafruitControlPort) -Exe $toolPath)
                }
                $resolved = Resolve-AutoBootloader -DfuToolPath $toolPath
            }
            if (-not $resolved.Resolved) {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'dfu-wait' -ExitCode 1 -Output ('Auto-detect probed for known nRF52 bootloaders ({0}); none visible on host. Check that the board is plugged in, that the user firmware honors 1200 bps touch, and that no other process is holding the COM port open.' -f $resolved.ProbedCandidates) -Exe $toolPath)
            }
            Write-NiusDetail ('[nius] auto-detect resolved to {0} ({1}:{2}, {3})' -f $resolved.Kind.ToUpper(), $resolved.Vid, $resolved.Pid, $resolved.Note)
            $BootloaderMode = $resolved.Kind
            $UsbVid = $resolved.Vid
            $UsbPid = $resolved.Pid
            $Uf2FamilyId = $resolved.Family
            $Uf2VolumeLabel = $resolved.VolumeLabel
            $Uf2Model = $resolved.Model
            $Uf2BoardId = $resolved.BoardId
            # Touch already attempted (or skipped because the device was already in
            # bootloader mode); don't re-touch on the legacy path below.
            $UseTouch1200 = 'false'
        }

        if ($BootloaderMode -eq 'uf2' -or $BootloaderMode -eq 'adafruit-dfu') {
            Assert-InputArtifact -Path $Hex -Label 'hex'
        } else {
            Assert-InputArtifact -Path $Bin -Label 'bin'
        }

        $touchPrepared = $false
        $bootloaderTransitionConfirmed = $false
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
                -BootloaderVid $(if ($runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu') { $UsbVid } else { '' }) `
                -BootloaderPid $(if ($runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu') { $UsbPid } else { '' }) `
                -RuntimeVid $(if ($runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu') { $effectiveRuntimeUsbVid } else { '' }) `
                -RuntimePid $(if ($runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu') { $effectiveRuntimeUsbPid } else { '' }) `
                -ExpectedLabel $(if ($runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu') { $Uf2VolumeLabel } else { '' }) `
                -ExpectedModel $(if ($runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu') { $Uf2Model } else { '' }) `
                -ExpectedBoardId $(if ($runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu') { $Uf2BoardId } else { '' })
            if ($touchTransitionResult.Triggered) {
                $touchPrepared = $true
                $bootloaderTransitionConfirmed = $true
                if ($touchTransitionResult.Candidate) {
                    Write-NiusDetail ('[nius] 1200 bps touch path confirmed via {0}.' -f $touchTransitionResult.Candidate.Label) -ForegroundColor DarkGray
                }
            }
            else {
                Write-NiusDetail ('[warn] Unable to confirm 1200 bps touch on {0}; proceeding with a direct serial DFU attempt in case the board is already in bootloader mode.' -f $adafruitControlPort) -ForegroundColor DarkYellow
            }
            if ($touchPrepared -and -not $bootloaderTransitionConfirmed -and $BootloaderMode -eq 'adafruit-dfu' -and $env:NIUS_ENABLE_SECOND_TOUCH_PASS -eq '1') {
                $halfPost = [int][Math]::Max(900, [Math]::Floor((Get-NiusPostTouchSleepMilliseconds) * 0.42))
                Start-Sleep -Milliseconds $halfPost
                Write-NiusDetail '[nius] Automatic second 1200bps touch pass (buttonless path; firmware debounce / USB settle).' -ForegroundColor DarkGray
                if (-not (Touch-SerialPort1200 -PortName $adafruitControlPort)) {
                    Write-NiusDetail ('[warn] Second touch pass failed on {0} (USB may be re-enumerating); continuing with post-touch wait.' -f $adafruitControlPort) -ForegroundColor DarkYellow
                }
            }
        } else {
            Write-NiusDetail '[nius] Entering bootloader (1200 bps touch)...' -ForegroundColor DarkGray
        }

        # Wait for the board to enter bootloader after the 1200 bps touch.
        if ($touchPrepared -and $runtimeSharesUploadIdentity -and $BootloaderMode -eq 'adafruit-dfu' -and -not $bootloaderTransitionConfirmed) {
            try {
                Wait-SerialPortResetCycle -PortName $adafruitControlPort -Purpose '1200 bps touch reset cycle'
            }
            catch {
                Write-NiusDetail ('[warn] No confirmed USB reset cycle observed on {0} after touch: {1}' -f $adafruitControlPort, $_.Exception.Message) -ForegroundColor DarkYellow
                Write-NiusDetail ('[nius] Continuing with a direct serial DFU attempt on {0} because this board reuses the same COM identity in runtime and bootloader.' -f $adafruitControlPort) -ForegroundColor DarkGray
            }
        }
        if ($touchPrepared) {
            if ($env:NIUS_FORCE_FIXED_POST_TOUCH_SLEEP -eq '1') {
                Start-Sleep -Milliseconds (Get-NiusPostTouchSleepMilliseconds)
            }
            else {
                # Adaptive settle: proceed the moment the bootloader COM is
                # stably openable instead of always burning the full window.
                Wait-NiusBootloaderPortSettled -PortName $adafruitControlPort -CeilingMs (Get-NiusPostTouchSleepMilliseconds)
            }
        }
        if ($touchPrepared -and $BootloaderMode -eq 'adafruit-dfu') {
            Write-NiusDetail '[nius] DFU: progress ~90% only means nrfutil is in serial DFU wait/transfer (host-side); MCU may still be in application if 1200/DTR reset did not arm yet).' -ForegroundColor DarkGray
        }

        # adafruit-dfu programs over the CDC interface; the host-side MSC LUN
        # is occasionally not promoted to a DiskDrive on Windows, but the CDC
        # path is unaffected. adafruit-nrfutil opens the COM port directly, so
        # we skip Wait-ForUsbBootloader / dfu-util probes for this transport.
        if ($BootloaderMode -eq 'adafruit-dfu') {
            $normalizedSdReq = ''
            if (-not [string]::IsNullOrWhiteSpace($SdReq)) {
                $normalizedSdReq = $SdReq.Trim().ToUpperInvariant()
            }
            $wildcardAttempted = $normalizedSdReq -eq '0XFFFE'

            # The byte-progress bar (driven from inside Invoke-AdafruitDfuDeploy)
            # owns the visible 0..100 range; no coarse 50% pre-stage here.
            Write-NiusDetail '[nius] Starting Adafruit serial DFU transfer...' -ForegroundColor DarkGray
            try {
                Invoke-AdafruitDfuDeploy -HexPath $Hex -Port $adafruitControlPort -SdReq $SdReq
            } catch {
                $fail = Get-NiusUploadFailureFromCatch -ErrorRecord $_
                $lastAdafruitError = $_
                $recoveredAfterRetouch = $false
                if ($UseTouch1200 -eq 'true' -and (Test-NiusAdafruitDfuFailureNeedsAutoRetouch -Failure $fail)) {
                    Write-NiusDetail '[nius] Serial DFU failed; automatic re-touch + one repeat (same sd-req, buttonless recovery).' -ForegroundColor Yellow
                    $null = Invoke-NiusAdafruitDfuRetouchWait `
                        -PortName $adafruitControlPort `
                        -Reason 'Auto re-touch before repeating adafruit-nrfutil serial DFU' `
                        -BootloaderVid $UsbVid `
                        -BootloaderPid $UsbPid `
                        -RuntimeVid $effectiveRuntimeUsbVid `
                        -RuntimePid $effectiveRuntimeUsbPid `
                        -ExpectedLabel $Uf2VolumeLabel `
                        -ExpectedModel $Uf2Model `
                        -ExpectedBoardId $Uf2BoardId
                    try {
                        Invoke-AdafruitDfuDeploy -HexPath $Hex -Port $adafruitControlPort -SdReq $SdReq
                        $recoveredAfterRetouch = $true
                    } catch {
                        $fail = Get-NiusUploadFailureFromCatch -ErrorRecord $_
                        $lastAdafruitError = $_
                        $recoveredAfterRetouch = $false
                    }
                }

                if ($recoveredAfterRetouch) {
                    # First-path success after automatic retry
                }
                else {
                    if (Test-NiusAdafruitDfuFailureIsTimeout -Failure $fail) {
                        Write-NiusDetail '[nius] Adafruit serial DFU timeout/stall is terminal for this run; skipping extra retries and surfacing the error immediately.' -ForegroundColor DarkYellow
                        if ($fail) {
                            Throw-NiusUploadFailure $fail
                        }
                        if ($null -ne $lastAdafruitError) {
                            throw $lastAdafruitError
                        }
                        throw
                    }
                    if ($wildcardAttempted) {
                        throw
                    }
                    $uploadFailure = $fail
                    if ($uploadFailure -and $uploadFailure.PSObject.Properties['Summary']) {
                        Write-NiusDetail ('[nius] Adafruit serial DFU transfer failed before wildcard retry: {0}' -f $uploadFailure.Summary)
                        foreach ($detail in @($uploadFailure.Details)) {
                            Write-NiusDetail ('[nius]   {0}' -f $detail)
                        }
                    }
                    else {
                        Write-NiusDetail ('[nius] Adafruit serial DFU transfer failed before wildcard retry: {0}' -f $_.Exception.Message)
                    }
                    Write-NiusDetail '[nius] Adafruit serial DFU transfer failed; retrying with wildcard sd-req 0xFFFE'
                    Write-Stage -Percent 90 -Label 'Finalizing'
                    if ($UseTouch1200 -eq 'true') {
                        $null = Invoke-NiusAdafruitDfuRetouchWait -PortName $adafruitControlPort -Reason 'Re-arming 1200 bps before wildcard sd-req retry (board may have left bootloader after failed transfer)'
                    }
                    Invoke-AdafruitDfuDeploy -HexPath $Hex -Port $adafruitControlPort -SdReq '0xFFFE'
                    $wildcardAttempted = $true
                    $normalizedSdReq = '0XFFFE'
                }
            }
            if ($env:NIUS_SKIP_POST_VERIFY -eq '1') {
                Write-NiusDetail '[nius] NIUS_SKIP_POST_VERIFY=1 - skipping bootloader/runtime PnP check (clone hosts where transfer succeeds but VID/PID transition is not detected)'
                Write-NiusUploadComplete
                exit 0
            }
            # Auto-skip post-verify when runtime + bootloader share VID:PID
            # (the typical Adafruit-fork clone configuration where
            # runtime_usb_pid == upload.usb_pid). The PnP transition the
            # post-verify waits for cannot fire because Windows sees the
            # same composite identity throughout; the wait would just burn
            # tens of seconds before timing out. The DFU streaming above
            # already confirmed success at the protocol level; if the host
            # also wants a "board is now in user mode" confirmation it can
            # set NIUS_FORCE_POST_VERIFY=1 to opt back in.
            if ($runtimeSharesUploadIdentity -and $env:NIUS_FORCE_POST_VERIFY -ne '1') {
                Write-NiusDetail ('[nius] Same-PID runtime/bootloader ({0}); skipping PnP post-verify (set NIUS_FORCE_POST_VERIFY=1 to override)' -f $UsbPid)
                Write-NiusUploadComplete
                exit 0
            }
            Write-Stage -Percent 94 -Label 'Verifying'
            $postUploadState = Wait-ForAdafruitRuntimeTransition -BootloaderVid $UsbVid -BootloaderPid $UsbPid -RuntimeVid $(if ($expectedRuntimeIdentity) { $expectedRuntimeIdentity.Vid } else { '' }) -RuntimePid $(if ($expectedRuntimeIdentity) { $expectedRuntimeIdentity.Pid } else { '' })
            if (-not $postUploadState.Success) {
                if (-not $wildcardAttempted -and $normalizedSdReq -ne '0XFFFE') {
                    Write-NiusDetail '[nius] post-verify still sees bootloader; retrying Adafruit serial DFU with wildcard sd-req 0xFFFE'
                    Write-Stage -Percent 95 -Label 'Finalizing'
                    if ($UseTouch1200 -eq 'true') {
                        $null = Invoke-NiusAdafruitDfuRetouchWait -PortName $adafruitControlPort -Reason 'Re-arming 1200 bps before post-verify wildcard retry'
                    }
                    Invoke-AdafruitDfuDeploy -HexPath $Hex -Port $adafruitControlPort -SdReq '0xFFFE'
                    $wildcardAttempted = $true
                    $normalizedSdReq = '0XFFFE'
                    Write-Stage -Percent 96 -Label 'Verifying'
                    $postUploadState = Wait-ForAdafruitRuntimeTransition -BootloaderVid $UsbVid -BootloaderPid $UsbPid -RuntimeVid $(if ($expectedRuntimeIdentity) { $expectedRuntimeIdentity.Vid } else { '' }) -RuntimePid $(if ($expectedRuntimeIdentity) { $expectedRuntimeIdentity.Pid } else { '' })
                }
            }
            if (-not $postUploadState.Success) {
                $uf2Fallback = $null
                if (-not [string]::IsNullOrWhiteSpace($Uf2FamilyId)) {
                    Write-Stage -Percent 97 -Label 'Finalizing'
                    $uf2Fallback = Wait-ForUf2Volume -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId -Attempts 30 -DelayMs 500
                }

                if ($uf2Fallback) {
                    Write-NiusDetail ('[nius] UF2 fallback: mounted volume detected at {0}; copying UF2 payload' -f $uf2Fallback.Drive)
                    Write-Stage -Percent 98 -Label 'Uploading'
                    Invoke-Uf2Deploy -HexPath $Hex -FamilyId $Uf2FamilyId -DrivePath $uf2Fallback.Drive
                    Write-Stage -Percent 99 -Label 'Verifying'
                    $postFallbackState = Wait-ForAdafruitRuntimeTransition -BootloaderVid $UsbVid -BootloaderPid $UsbPid -RuntimeVid $(if ($expectedRuntimeIdentity) { $expectedRuntimeIdentity.Vid } else { '' }) -RuntimePid $(if ($expectedRuntimeIdentity) { $expectedRuntimeIdentity.Pid } else { '' })
                    if ($postFallbackState.Success) {
                        Write-NiusUploadComplete
                        exit 0
                    }

                    $combinedSummary = ('{0}; uf2_fallback={1}; post_fallback={2}' -f $postUploadState.Summary, $uf2Fallback.Drive, $postFallbackState.Summary)
                    Throw-NiusUploadFailure (New-UploadFailure -Kind 'post-verify' -ExitCode 1 -Output $combinedSummary -Exe $toolPath)
                }

                $summary = ('{0}; uf2_fallback=not-mounted' -f $postUploadState.Summary)
                if (-not [string]::IsNullOrWhiteSpace($Uf2FamilyId)) {
                    $uf2Artifact = New-Uf2Artifact -HexPath $Hex -FamilyId $Uf2FamilyId
                    $summary = ('{0}; uf2_artifact={1}' -f $summary, $uf2Artifact)
                }
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'post-verify' -ExitCode 1 -Output $summary -Exe $toolPath)
            }
            Write-NiusUploadComplete
            exit 0
        }

        $detectedBootloader = $null
        if ($WaitForUploadPort -eq 'true') {
            Write-Stage -Percent 32 -Label 'Connecting'
            $detectedBootloader = Wait-ForUsbBootloader -Exe $toolPath -UsbVid $UsbVid -UsbPid $UsbPid -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId
            if (-not $detectedBootloader) {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'dfu-wait' -ExitCode 1 -Output '' -Exe $toolPath)
            }
        } else {
            Write-Stage -Percent 32 -Label 'Connecting'
            $detectedBootloader = [pscustomobject]@{
                Kind = if ($BootloaderMode -eq 'uf2') { 'uf2' } else { 'dfu' }
                Summary = if ($BootloaderMode -eq 'uf2') { Get-Uf2ProbeSummary -ExpectedLabel $Uf2VolumeLabel -ExpectedModel $Uf2Model -ExpectedBoardId $Uf2BoardId } else { $null }
            }
        }

        if ($BootloaderMode -eq 'uf2') {
            if ($detectedBootloader.Kind -ne 'uf2') {
                Throw-NiusUploadFailure (New-UploadFailure -Kind 'dfu' -ExitCode 1 -Output ('Selected bootloader mode requires UF2, but the board presented Nordic DFU {0}:{1} instead.' -f $UsbVid, $UsbPid) -Exe $toolPath)
            }

            Write-Stage -Percent 68 -Label 'Uploading'
            Invoke-Uf2Deploy -HexPath $Hex -FamilyId $Uf2FamilyId -DrivePath $detectedBootloader.Summary.Drive
            Write-NiusUploadComplete
            exit 0
        }

        if ($detectedBootloader.Kind -eq 'uf2') {
            Throw-NiusUploadFailure (New-UploadFailure -Kind 'dfu' -ExitCode 1 -Output ('Selected bootloader mode requires Nordic DFU, but the board presented UF2 volume {0} with model "{1}" and board-id "{2}" instead.' -f $detectedBootloader.Summary.Drive, $detectedBootloader.Summary.Model, $detectedBootloader.Summary.BoardId) -Exe $toolPath)
        }

        Write-Stage -Percent 68 -Label 'Uploading'
        Invoke-CommandChecked -Exe $toolPath -Arguments @('-v', '-d', ('{0}:{1}' -f $UsbVid, $UsbPid), '-a', $Alt, '-D', $Bin, '-R') -FailureKind 'dfu' -ProgressPercent 78 -ProgressLabel 'Nordic DFU transfer active'
        Write-NiusUploadComplete
        exit 0
    }

    Assert-InputArtifact -Path $Hex -Label 'hex'
    Write-Stage -Percent 10 -Label 'Connecting'
    Write-Stage -Percent 42 -Label 'Uploading'
    Invoke-CommandChecked -Exe $toolPath -Arguments @('-s', $ScriptRoot, '-f', $Config, '-c', ('init; halt; program {{{0}}} verify reset exit' -f $Hex)) -FailureKind 'openocd' -ProgressPercent 76 -ProgressLabel 'SWD flash transaction active'
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
