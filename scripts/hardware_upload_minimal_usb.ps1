#Requires -Version 5.1
<#
.SYNOPSIS
  Upload MinimalUsbSmoke after manual bootloader entry (single USB ProMicro clone).

.NOTES
  Sets NIUS_SKIP_POST_VERIFY=1 so upload.ps1 does not wait for Windows PnP to see PID 0x00B4.
  Run THIS script directly in PowerShell — do NOT nest `powershell -Command "$env:..."` or the
  variable will be stripped before the parameter binding sees it.

  Use -UseArduinoCiConfig when your global Arduino15 sketchbook does NOT contain this core:
  the script will ensure `.arduino-ci-sketchbook` junction + pass `--config-file .arduino-ci.yaml`
  (same layout as `scripts/usb_single_cable_ci.ps1`).

  **Single COM while CDC-disabled (`-MinimalUsb`)**
    Applies fqbn options `usbcdc=disabled,usbdesc=no_app_dfu`: descriptor exposes the fixed Service CDC pair only
    (no second User CDC, no runtime app DFU interface). Windows normally enumerates **one** CDC COM in application mode.
    Core routes Arduino `Serial` to that Service CDC; `platform.txt` still passes `-UseTouch1200 true` into upload.ps1,
    so the **same COM** remains valid for soft-reset → bootloader and adafruit-nrfutil serial DFU.     IDE Tools → Port
    must be that COM (often identical assignment before/after flash on clone 0x239A:0x00B3 stacks).

    **Dual CDC (`usbcdc` Enabled)** Adafruit serial DFU must use the **SERVICE / MI_00** COM. Selecting the **USER CDC**
    COM fails fast by default (`upload.ps1`); set `NIUS_ALLOW_USER_CDC_UPLOAD_PORT=1` only if you need legacy auto-remap.

.EXAMPLE
  .\scripts\hardware_upload_minimal_usb.ps1 -Port COM3 -BootloaderMenu promicroserialnosd -UseArduinoCiConfig -MinimalUsb
  .\scripts\hardware_upload_minimal_usb.ps1 -Port COM3 -BootloaderMenu promicroserial
  .\scripts\hardware_upload_minimal_usb.ps1 -Port COM3 -BootloaderMenu promicroseriallegacy
  .\scripts\hardware_upload_minimal_usb.ps1 -Port COM3 -BootloaderMenu promicroserialnosd
  .\scripts\hardware_upload_minimal_usb.ps1 -Port COM3 -MinimalUsb
  # Single COM + CDC-disabled + automatic bootloader touch on the same port:
  .\scripts\hardware_upload_minimal_usb.ps1 -Port COM3 -BootloaderMenu promicroserialnosd -UseArduinoCiConfig -MinimalUsb
  .\scripts\hardware_upload_minimal_usb.ps1 -Port COM3 -BootloaderMenu promicroserialnosd -DualCdcUsbGdbStub
  Incremental compile by default; pass -CleanBuild for arduino-cli compile --clean.

  若未设置环境变量，本脚本会写入与验证脚本一致的 DFU/touch 默认值（含 `NIUS_POST_TOUCH_SLEEP_MS=3800`；机器慢可自行加大）。
  其它与二次上传相关：`NIUS_SKIP_STOP_NRFUTIL_BEFORE_TOUCH`、`NIUS_SKIP_AUTO_DFU_RETOUCH_RETRY`、`NIUS_SINGLE_TOUCH_ONLY`；**除非确信已在 bootloader**，不要将 `NIUS_ALLOW_SKIP_TOUCH_IF_BOOTLOADER_PORT=1` 用在同一 PID 的克隆板上。
#>
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [ValidateSet('promicroserial', 'promicroseriallegacy', 'promicroserialnosd')]
    [string]$BootloaderMenu = 'promicroserialnosd',
    [string]$ExtraBoardOptions = '',
    [ValidateRange(0, 23)]
    [int]$UsbdDiagStage = 0,
    [switch]$MinimalUsb,
    [switch]$DualCdcUsbGdbStub,
    [switch]$UseArduinoCiConfig,
    [switch]$CleanBuild
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path $PSScriptRoot -Parent
$Sketch = Join-Path $RepoRoot 'examples\MinimalUsbSmoke'
if (-not (Test-Path $Sketch)) {
    throw "Sketch not found: $Sketch"
}

$NiusVerifyCache = Join-Path $RepoRoot '.nius-verify-cache'
New-Item -ItemType Directory -Force -Path $NiusVerifyCache | Out-Null
$NiusHwCliLog = Join-Path $NiusVerifyCache 'hardware_upload_cli.log'

function Read-NiusCliRedirectFile {
    param([string]$LiteralPath)
    if (-not (Test-Path -LiteralPath $LiteralPath)) {
        return ''
    }
    try {
        return (Get-Content -LiteralPath $LiteralPath -Raw -Encoding Unicode)
    }
    catch {
        try {
            return (Get-Content -LiteralPath $LiteralPath -Raw -Encoding UTF8)
        }
        catch {
            return (Get-Content -LiteralPath $LiteralPath -Raw)
        }
    }
}

function Invoke-ArduinoCliCaptureAllStreams {
    param(
        [Parameter(Mandatory)][string]$Stage,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Arguments
    )

    "`r`n===== $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $Stage =====`r`n" | Out-File -LiteralPath $NiusHwCliLog -Append -Encoding utf8

    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ('nius-hw-cli-{0}.log' -f [Guid]::NewGuid().ToString('N'))
    # arduino-cli writes its verbose upload-tool wrapper (the niusrobotlab
    # banner + [nius] lines + USER-CDC rejection message) to stderr. PS 5.1
    # wraps every native-exe stderr line as a NativeCommandError, and this
    # script sets $ErrorActionPreference='Stop' at top-level, so without a
    # localized Continue the very first banner character would terminate the
    # pipeline before *> $tmp can fill the temp file. Localize Continue
    # around the arduino-cli call only.
    $savedErrPref = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & arduino-cli @Arguments *> $tmp
        $code = $LASTEXITCODE
        $blob = Read-NiusCliRedirectFile -LiteralPath $tmp
        if (-not [string]::IsNullOrEmpty($blob)) {
            [System.IO.File]::AppendAllText($NiusHwCliLog, $blob, [System.Text.UTF8Encoding]::new($false))
            Write-Host $blob
            try {
                [Console]::Out.Flush()
            }
            catch {
            }
        }
        return [int]$code
    }
    finally {
        $ErrorActionPreference = $savedErrPref
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
    }
}

function Get-NiusDescendantProcessIds {
    param([int]$RootPid)

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
    $found = New-Object System.Collections.Generic.List[int]

    while ($pending.Count -gt 0) {
        $parentId = $pending.Dequeue()
        if (-not $childrenByParent.ContainsKey($parentId)) {
            continue
        }
        foreach ($childId in $childrenByParent[$parentId]) {
            if ($childId -eq $PID -or $found.Contains($childId)) {
                continue
            }
            $found.Add($childId)
            $pending.Enqueue($childId)
        }
    }

    return $found.ToArray()
}

function Stop-NiusDescendantProcesses {
    param([int]$RootPid = $PID)

    $ids = @(Get-NiusDescendantProcessIds -RootPid $RootPid)
    [Array]::Reverse($ids)
    foreach ($childId in $ids) {
        try {
            $proc = Get-Process -Id $childId -ErrorAction Stop
            Stop-Process -Id $childId -Force -ErrorAction Stop
            Write-Host ("[hw-upload] cleaned child PID {0} ({1})" -f $childId, $proc.ProcessName) -ForegroundColor DarkGray
        }
        catch {
        }
    }
}

$ArduinoCliPrefix = @()
if ($UseArduinoCiConfig) {
    . (Join-Path $PSScriptRoot 'arduino_cli_with_repo.ps1')
    $ArduinoCliPrefix = @(Get-ArduinoCliRepoConfigArgs -EnsureJunction)
}

if ($MinimalUsb -and $DualCdcUsbGdbStub) {
    throw 'Cannot use -MinimalUsb together with -DualCdcUsbGdbStub'
}

if ($MinimalUsb) {
    $ExtraBoardOptions = 'usbcdc=disabled,usbdesc=no_app_dfu'
}
elseif ($DualCdcUsbGdbStub) {
    $suffix = 'usbcdc=enabled,buildprofile=usbgdbstub'
    if ([string]::IsNullOrWhiteSpace($ExtraBoardOptions)) {
        $ExtraBoardOptions = $suffix
    }
    else {
        $ExtraBoardOptions = "$ExtraBoardOptions,$suffix"
    }
}

$env:NIUS_SKIP_POST_VERIFY = '1'
if ([string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS)) {
    $env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS = '12000'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_DFU_PROCESS_TIMEOUT_MS)) {
    $env:NIUS_ADAFRUIT_DFU_PROCESS_TIMEOUT_MS = '180000'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS)) {
    $env:NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS = '30000'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_POST_TOUCH_SLEEP_MS)) {
    $env:NIUS_POST_TOUCH_SLEEP_MS = '3800'
}
$fqbn = "arduinonrf:nrf52:promicro_nrf52840:bootloader=$BootloaderMenu"
if (-not [string]::IsNullOrWhiteSpace($ExtraBoardOptions)) {
    $fqbn = "$fqbn,$ExtraBoardOptions"
}

$sessionHeader = @"
[hw-upload] arduino-cli full log (stdout+stderr): $NiusHwCliLog
Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
Port=$Port
fqbn=$fqbn

"@
[System.IO.File]::WriteAllText($NiusHwCliLog, $sessionHeader, [System.Text.UTF8Encoding]::new($false))
Write-Host "[hw-upload] arduino-cli output -> $NiusHwCliLog (IDE transcript often misses child output)" -ForegroundColor DarkGray

try {
    Write-Host "[hw-upload] NIUS_SKIP_POST_VERIFY=$($env:NIUS_SKIP_POST_VERIFY)" -ForegroundColor Cyan
    Write-Host "[hw-upload] compile $fqbn$(if ($CleanBuild) { ' (--clean)' } else { '' })" -ForegroundColor Cyan
    $compileArgs = @($ArduinoCliPrefix + @('compile', '--fqbn', $fqbn))
    if ($CleanBuild) {
        $compileArgs += '--clean'
    }
    if ($UsbdDiagStage -gt 0) {
        $compileArgs += @('--build-property', "build.debug_flags=-DNRF_USBD_DIAG_RESET_STAGE=$UsbdDiagStage")
    }
    $compileArgs += $Sketch
    & arduino-cli @compileArgs
    if ($LASTEXITCODE -ne 0) {
        throw "compile failed exit=$LASTEXITCODE"
    }

    Write-Host "[hw-upload] upload -p $Port" -ForegroundColor Cyan
    try {
        [Console]::Out.Flush()
    }
    catch {
    }
    # Use Invoke-ArduinoCliCaptureAllStreams (file-redirect via `*> tmp`) so
    # the verify harness's Assert-TextMatch can read upload.ps1's rejection
    # text (e.g. "Wrong COM for Adafruit serial DFU" on V2's USER-CDC path)
    # from $NiusHwCliLog. Direct `& arduino-cli ...` passes the grandchild
    # pwsh's Write-Host straight to the console where Start-Transcript on
    # the parent does not capture it.
    $uploadArgs = @($ArduinoCliPrefix + @('upload', '-p', $Port, '--fqbn', $fqbn, '-v', $Sketch))
    $uploadExit = Invoke-ArduinoCliCaptureAllStreams -Stage 'upload' -Arguments $uploadArgs
    if ($uploadExit -ne 0) {
        throw "upload failed exit=$uploadExit"
    }

    if ($env:NIUS_SKIP_BOARD_LIST_AFTER_UPLOAD -eq '1') {
        Write-Host '[hw-upload] skip board list (NIUS_SKIP_BOARD_LIST_AFTER_UPLOAD=1)' -ForegroundColor DarkGray
    }
    else {
        Write-Host '[hw-upload] board list after upload:' -ForegroundColor Cyan
        & arduino-cli @ArduinoCliPrefix board list
    }

    Write-Host '[hw-upload] Done. If no CDC appears, compare BootloaderMenu promicroserial (0x26000) or promicroseriallegacy (0x27000).' -ForegroundColor Green
    exit 0
}
finally {
    Stop-NiusDescendantProcesses -RootPid $PID
}
