#Requires -Version 5.1
<#
.SYNOPSIS
  ProMicro nRF52840 USB 串口 DFU 行为三条验证（按顺序做；任一失败先修到通过再继续）。

.PARAMETER Port
  可选：维护口（SERVICE CDC / MI 最小）的 COM。不传则自动枚举。单独跑 -Phase V3 时若上一进程已跑过 V2，
  会读取仓库下 .nius-verify-cache 里记录的维护口；也可手动 -Port 指定「旧口」。

.NOTES
  === 验证 1（V1）流程 — 按顺序执行 ===
  目的：证明 **usbcdc=disabled**（`-MinimalUsb`，FQBN 含 `usbcdc=disabled,usbdesc=no_app_dfu`）时，
        可在 **同一维护口（SERVICE CDC）** 上 **连续两次** `compile + upload`，无需用户 CDC 口。

  维护口从哪来：
  - 不传 `-Port`：按板 `promicro_nrf52840`（VID/PID 239A:00B3）解析当前 **SERVICE / 维护** COM。
  - 传 `-Port COMx`：全程强制使用该 COM（仍然是「同一口」前提，只是不让脚本自动猜）。

  V1 脚本内部步骤（与控制台 `[verify] V1-x/y` 标题一致）：
  1/4 **Pass A**：`hardware_upload_minimal_usb.ps1 -MinimalUsb` → 一次 arduino-cli compile + 一次 upload；
             失败则杀 nrfutil、间隔 500ms，最多重试 3 次。
  2/4 settle：`UsbSettleSeconds`（默认 1s，可用参数加大）+ 再杀 adafruit-nrfutil。
  3/4 **Pass B**：对 **同一维护口** 再来一轮完整 compile + upload（这就是俗称的「第二次上传」）。
             同样带重试；**Pass B 成功**才表示「同一维护口第二次完整 compile+upload」成立。
  4/4 **断言**：Pass B 完成后枚举运行时串口，必须 **恰好 1 个 COM**（单维护口用户态），否则抛错退出。
        （Pass A 后若 COM 数不是 1 仅打印提示，不失败 —— 最终以 Pass B 后为准。）

  跑法：`.\scripts\verify_promicro_usbcdc_upload_behavior.ps1 -Phase V1 -UseArduinoCiConfig`
  `-Phase All` 时会在进入 V1 前删除旧的 `.nius-verify-cache`，避免与 V2/V3 残留串台。
  `hardware_upload_minimal_usb.ps1` 在 **同一 PowerShell 会话**中直接调用，不经嵌套 `powershell.exe`、不重定向标准流。
  **全程会话日志**（含 Write-Host / arduino-cli / upload 输出）写入 `.nius-verify-cache/verify_last_run.log`；IDE 不弹出终端时直接在资源管理器或编辑器中打开该文件即可回看进度与第二次上传结果。

  约定流程（与 IDE 选口一致）：
  1) 验证1：CDC disable 固件连续两次从同一维护口烧录；出错则修脚本/上传逻辑直到通过。
     通过后板子应处于 **usbcdc=disabled 的用户态**，证明一根 USB 在用户模式下可维护烧录。
  2) 验证2：从维护口烧录 CDC enable；再用「新出现的用户 CDC 口」尝试烧录 enable，必须失败并提示换维护口。
     通过后板子处于 **usbcdc=enabled（双 COM）**，并锁定此时的 **旧维护口 COM** 供验证3。
  3) 验证3：只用 **验证2 锁定的那份旧 COM（维护口）** 先后烧录 enable 与 disable；禁用后恢复单 COM。

  分包运行示例：
    .\scripts\verify_promicro_usbcdc_upload_behavior.ps1 -Phase V1 -UseArduinoCiConfig
    .\scripts\verify_promicro_usbcdc_upload_behavior.ps1 -Phase V2 -UseArduinoCiConfig
    .\scripts\verify_promicro_usbcdc_upload_behavior.ps1 -Phase V3 -UseArduinoCiConfig
  单次串联：-Phase All

  未设置环境变量时本脚本将 NIUS_ADAFRUIT_DFU_PROCESS_TIMEOUT_MS=180000、NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS=12000、NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS=90000、NIUS_POST_TOUCH_SLEEP_MS=3800，
  并设置 NIUS_SKIP_BOARD_LIST_AFTER_UPLOAD=1 以跳过每次上传后的 `arduino-cli board list`。每条上传最多重试 3 次。

  纯 arduino-cli 链（PowerShell 5 不要用 &&）：`scripts/arduino_cli_upload_promicro_minimal_usb_disabled.ps1 -Port COMx -UseArduinoCiConfig`
#>
param(
    [string]$Port = '',
    [string]$BootloaderMenu = 'promicroserialnosd',
    [switch]$UseArduinoCiConfig,
    [int]$UsbSettleSeconds = 1,
    [ValidateSet('V1', 'V2', 'V3', 'All')]
    [string]$Phase = 'All'
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $Repo

if ([string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_DFU_PROCESS_TIMEOUT_MS)) {
    $env:NIUS_ADAFRUIT_DFU_PROCESS_TIMEOUT_MS = '180000'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_SKIP_BOARD_LIST_AFTER_UPLOAD)) {
    $env:NIUS_SKIP_BOARD_LIST_AFTER_UPLOAD = '1'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS)) {
    $env:NIUS_ADAFRUIT_WAIT_SERIAL_READY_MS = '12000'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS)) {
    $env:NIUS_ADAFRUIT_DFU_IDLE_TIMEOUT_MS = '90000'
}
if ([string]::IsNullOrWhiteSpace($env:NIUS_POST_TOUCH_SLEEP_MS)) {
    $env:NIUS_POST_TOUCH_SLEEP_MS = '3800'
}

. (Join-Path $Repo 'hardware/arduinonrf/nrf52/tools/niusrobotlab/usb_port_helpers.ps1')

$VerifyStateDir = Join-Path $Repo '.nius-verify-cache'
$VerifyOldComFile = Join-Path $VerifyStateDir 'promicro_old_service_com.txt'
$script:NiusVerifyTranscriptLog = Join-Path $VerifyStateDir 'verify_last_run.log'
$script:VerifyOldServiceComForV3 = $null
$script:NiusUserExplicitServiceCom = $null
if ($PSBoundParameters.ContainsKey('Port') -and -not [string]::IsNullOrWhiteSpace($Port)) {
    $script:NiusUserExplicitServiceCom = $Port.Trim()
}

New-Item -ItemType Directory -Force -Path $VerifyStateDir | Out-Null
try {
    Stop-Transcript | Out-Null
}
catch {
}
try {
    Start-Transcript -LiteralPath $script:NiusVerifyTranscriptLog -Force | Out-Null
    Write-Host ("[verify] Session log (always refreshed): {0}" -f $script:NiusVerifyTranscriptLog) -ForegroundColor Cyan
}
catch {
    $script:NiusVerifyTranscriptLog = $null
    Write-Warning ('[verify] Start-Transcript failed; no log file. Error: {0}' -f $_)
}

function Get-ServiceCom {
    $p = Get-NiusServiceComForBoard -BoardName 'promicro_nrf52840'
    if (-not $p) {
        throw 'No runtime COM for board VID/PID 239A:00B3 (plug board / install driver).'
    }
    return $p
}

function Get-ServiceComOrExplicit {
    if ($null -ne $script:NiusUserExplicitServiceCom) {
        return $script:NiusUserExplicitServiceCom
    }
    return Get-ServiceCom
}

function Get-ComForV3OldService {
    if ($null -ne $script:NiusUserExplicitServiceCom) {
        return $script:NiusUserExplicitServiceCom
    }
    if (-not [string]::IsNullOrWhiteSpace($script:VerifyOldServiceComForV3)) {
        return [string]$script:VerifyOldServiceComForV3
    }
    if (Test-Path -LiteralPath $VerifyOldComFile) {
        $raw = (Get-Content -LiteralPath $VerifyOldComFile -Raw).Trim()
        if (-not [string]::IsNullOrWhiteSpace($raw)) {
            return $raw
        }
    }
    return Get-ServiceCom
}

function Stop-NiusAdafruitNrfutilQuiet {
    cmd /c "taskkill /F /IM adafruit-nrfutil.exe 2>nul" | Out-Null
}

function Invoke-SuccessHardwareMinimalUsbWithRetry {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$GetPort,
        [switch]$MinimalUsb,
        [Parameter(Mandatory = $true)][string]$StepLabel,
        [int]$MaxAttempts = 3
    )

    $r = $null
    foreach ($attempt in 1..$MaxAttempts) {
        Stop-NiusAdafruitNrfutilQuiet
        Start-Sleep -Milliseconds 500
        $com = & $GetPort
        $r = Invoke-HardwareMinimalUsbBuildUpload -Com $com -MinimalUsb:$MinimalUsb
        if ($r.ExitCode -eq 0) {
            break
        }
        Write-Host ("[verify] {0} attempt {1} failed exit {2}; retrying..." -f $StepLabel, $attempt, $r.ExitCode) -ForegroundColor Yellow
    }
    return $r
}

function Invoke-HardwareMinimalUsbBuildUpload {
    param(
        [Parameter(Mandatory = $true)][string]$Com,
        [switch]$MinimalUsb
    )

    $scriptPath = Join-Path $Repo 'scripts/hardware_upload_minimal_usb.ps1'
    $splat = @{
        Port           = $Com
        BootloaderMenu = $BootloaderMenu
    }
    if ($UseArduinoCiConfig) {
        $splat.UseArduinoCiConfig = $true
    }
    if ($MinimalUsb) {
        $splat.MinimalUsb = $true
    }

    Write-Host ("[verify] >>> RUN hardware_upload -Port {0}{1}" -f $Com, $(if ($MinimalUsb) { ' -MinimalUsb' } else { '' })) -ForegroundColor Cyan

    $exitCode = 0
    $text = ''
    try {
        & $scriptPath @splat
        if ($null -ne $LASTEXITCODE) {
            $exitCode = [int]$LASTEXITCODE
        }
    } catch {
        $exitCode = 1
        $text = [string]$_.Exception.Message
        if ($_.InvocationInfo -and $_.InvocationInfo.PositionMessage) {
            $text += [Environment]::NewLine + [string]$_.InvocationInfo.PositionMessage
        }
        if ($_.Exception.InnerException) {
            $text += [Environment]::NewLine + [string]$_.Exception.InnerException.Message
        }
    }

    if ($exitCode -lt 0) {
        $exitCode = 1
    }

    Write-Host ("[verify] <<< DONE hardware_upload exit={0}" -f $exitCode) -ForegroundColor $(if ($exitCode -eq 0) { 'Green' } else { 'Yellow' })

    return @{ ExitCode = $exitCode; Text = $text }
}

function Assert-ExitCode {
    param($Result, [int]$Expected, [string]$Label)

    if ($Result.ExitCode -ne $Expected) {
        Write-Host $Result.Text
        throw ("{0}: expected exit {1}, got {2}" -f $Label, $Expected, $Result.ExitCode)
    }
}

function Assert-TextMatch {
    param($Result, [string]$Pattern, [string]$Label)

    $hay = [string]$Result.Text
    $tp = $script:NiusVerifyTranscriptLog
    if (-not [string]::IsNullOrWhiteSpace($tp) -and (Test-Path -LiteralPath $tp)) {
        try {
            $hay += [Environment]::NewLine + (Get-Content -LiteralPath $tp -Raw -ErrorAction Stop)
        }
        catch {
        }
    }

    if ($hay -notmatch $Pattern) {
        Write-Host $Result.Text
        throw ("{0}: output missing pattern '{1}'" -f $Label, $Pattern)
    }
}

try {
    if ($null -ne $script:NiusUserExplicitServiceCom) {
        $Port = $script:NiusUserExplicitServiceCom
    }
    else {
        $Port = Get-ServiceCom
    }

    Write-Host ("[verify] Phase={0}  seed port (SERVICE/maintenance): {1}" -f $Phase, $Port) -ForegroundColor Cyan

    if ($Phase -eq 'V3' -and ($null -eq $script:NiusUserExplicitServiceCom) -and -not (Test-Path -LiteralPath $VerifyOldComFile) -and [string]::IsNullOrWhiteSpace($script:VerifyOldServiceComForV3)) {
        Write-Host '[verify] V3 hint: run V2 first or pass -Port <OLD SERVICE COM> (same COM before User CDC appeared).' -ForegroundColor DarkYellow
    }

    if ($Phase -eq 'All' -or $Phase -eq 'V1') {
        # V1: same maintenance COM, MinimalUsb (CDC off) — full compile+upload twice; see .NOTES "验证 1"
        $script:VerifyOldServiceComForV3 = $null
        if (Test-Path -LiteralPath $VerifyOldComFile) {
            Remove-Item -LiteralPath $VerifyOldComFile -Force -ErrorAction SilentlyContinue
            Write-Host '[verify] Removed stale .nius-verify-cache from a previous verify session.' -ForegroundColor DarkGray
        }
        Write-Host '[verify] V1-1/4: Pass A - first compile+upload (-MinimalUsb) on SAME maintenance COM...' -ForegroundColor Yellow
        $r1a = Invoke-SuccessHardwareMinimalUsbWithRetry -GetPort { Get-ServiceComOrExplicit } -MinimalUsb -StepLabel 'V1 pass A'
        Assert-ExitCode -Result $r1a -Expected 0 -Label 'V1 pass A'
        Write-Host '[verify] V1-2/4: USB settle + stop nrfutil...' -ForegroundColor DarkGray
        Start-Sleep -Seconds $UsbSettleSeconds
        Stop-NiusAdafruitNrfutilQuiet
        Start-Sleep -Milliseconds 800
        Stop-NiusAdafruitNrfutilQuiet

        $rankedAfterA = @(Get-NiusBoardRuntimeSerialPortsRankedByMi -BoardName 'promicro_nrf52840')
        if ($rankedAfterA.Count -ne 1) {
            Write-Host ('[verify] V1 note (non-fatal): after Pass A expected 1 COM; saw {0}: {1}' -f $rankedAfterA.Count, (($rankedAfterA | ForEach-Object { $_.Com }) -join ', '))
        }

        Write-Host '[verify] V1-3/4: Pass B - second compile+upload (same COM, same -MinimalUsb)...' -ForegroundColor Yellow
        $r1b = Invoke-SuccessHardwareMinimalUsbWithRetry -GetPort { Get-ServiceComOrExplicit } -MinimalUsb -StepLabel 'V1 pass B'
        Assert-ExitCode -Result $r1b -Expected 0 -Label 'V1 pass B'
        Write-Host '[verify] V1-4/4: assert exactly one runtime COM after Pass B...' -ForegroundColor DarkGray
        Start-Sleep -Seconds $UsbSettleSeconds
        $rankedAfterB = @(Get-NiusBoardRuntimeSerialPortsRankedByMi -BoardName 'promicro_nrf52840')
        if ($rankedAfterB.Count -ne 1) {
            throw ("V1 fail: after second disable flash expected 1 COM, got {0}" -f $rankedAfterB.Count)
        }
        Write-Host '[verify] V1 PASS' -ForegroundColor Green
        Write-Host '[verify] STATE after V1: usbcdc=DISABLED, user mode, single COM (USB Serial DFU chain OK).' -ForegroundColor Green
    }

    if ($Phase -eq 'All' -or $Phase -eq 'V2') {
        # --- Verification 2: CDC enabled; USER (high MI) COM must fail with guidance ---
        Write-Host '[verify] V2: flash CDC-enabled from maintenance COM; USER CDC upload must fail...' -ForegroundColor Yellow
        $r2a = Invoke-SuccessHardwareMinimalUsbWithRetry -GetPort { Get-ServiceComOrExplicit } -MinimalUsb:$false -StepLabel 'V2 enable CDC baseline flash'
        Assert-ExitCode -Result $r2a -Expected 0 -Label 'V2 enable CDC baseline flash'
        Start-Sleep -Seconds $UsbSettleSeconds

        $rankedDual = @(Get-NiusBoardRuntimeSerialPortsRankedByMi -BoardName 'promicro_nrf52840')
        if ($rankedDual.Count -lt 2) {
            throw ("V2 skip/fail: need >=2 COM after CDC-enabled firmware; got {0}. Check usbcdc build." -f $rankedDual.Count)
        }
        $userCom = [string]$rankedDual[$rankedDual.Count - 1].Com
        $svcCom = [string]$rankedDual[0].Com
        Write-Host ("[verify] V2 service={0} user(highest MI)={1}" -f $svcCom, $userCom) -ForegroundColor Cyan

        $r2b = Invoke-HardwareMinimalUsbBuildUpload -Com $userCom -MinimalUsb:$false
        Assert-ExitCode -Result $r2b -Expected 1 -Label 'V2 USER COM upload must fail'
        Assert-TextMatch -Result $r2b -Pattern 'Wrong COM for Adafruit serial DFU|USER CDC serial cannot|ZH: USER CDC' -Label 'V2 failure message'

        $script:VerifyOldServiceComForV3 = $svcCom
        New-Item -ItemType Directory -Force -Path $VerifyStateDir | Out-Null
        Set-Content -LiteralPath $VerifyOldComFile -Value $svcCom -Encoding ascii
        Write-Host ("[verify] Locked OLD/service COM for V3: {0} (written {1})" -f $svcCom, $VerifyOldComFile) -ForegroundColor Cyan
        Write-Host '[verify] STATE after V2: usbcdc=ENABLED (dual COM). Use ONLY this OLD COM for V3 uploads.' -ForegroundColor Green
        Write-Host '[verify] V2 PASS' -ForegroundColor Green
    }

    if ($Phase -eq 'All' -or $Phase -eq 'V3') {
        $v3ComLabel = Get-ComForV3OldService
        Write-Host ('[verify] V3: uploads ONLY on OLD/service COM = {0} (never the new User CDC).' -f $v3ComLabel) -ForegroundColor Yellow
        $r3a = Invoke-SuccessHardwareMinimalUsbWithRetry -GetPort { Get-ComForV3OldService } -MinimalUsb:$false -StepLabel 'V3 flash CDC enabled over OLD SERVICE COM'
        Assert-ExitCode -Result $r3a -Expected 0 -Label 'V3 flash CDC enabled over OLD SERVICE COM'

        Start-Sleep -Seconds $UsbSettleSeconds
        $rankedV3a = @(Get-NiusBoardRuntimeSerialPortsRankedByMi -BoardName 'promicro_nrf52840')
        if ($rankedV3a.Count -lt 2) {
            throw ("V3 fail: expected dual COM after enabled flash; got {0}" -f $rankedV3a.Count)
        }

        $r3b = Invoke-SuccessHardwareMinimalUsbWithRetry -GetPort { Get-ComForV3OldService } -MinimalUsb -StepLabel 'V3 flash CDC disabled over OLD SERVICE COM'
        Assert-ExitCode -Result $r3b -Expected 0 -Label 'V3 flash CDC disabled over OLD SERVICE COM'

        Start-Sleep -Seconds $UsbSettleSeconds
        $rankedV3b = @(Get-NiusBoardRuntimeSerialPortsRankedByMi -BoardName 'promicro_nrf52840')
        if ($rankedV3b.Count -ne 1) {
            throw ("V3 fail: after disable flash expected 1 COM; got {0}: {1}" -f $rankedV3b.Count, (($rankedV3b | ForEach-Object { $_.Com }) -join ','))
        }
        Write-Host '[verify] STATE after V3: usbcdc=DISABLED again, single COM.' -ForegroundColor Green
        Write-Host '[verify] V3 PASS' -ForegroundColor Green
    }

    if ($Phase -eq 'All') {
        Write-Host '[verify] All checks passed (V1+V2+V3).' -ForegroundColor Green
    }
    else {
        $next = switch ($Phase) {
            'V1' { 'Next: fix failures until V1 passes; then run -Phase V2. After V1 board is CDC-disabled user mode.' }
            'V2' { 'Next: fix until V2 passes; then run -Phase V3 using OLD COM (cached file or -Port). After V2 board is CDC-enabled dual COM.' }
            'V3' { 'All three done if you ran V1..V3 in order; board back to CDC-disabled single COM.' }
            Default { '' }
        }
        Write-Host ("[verify] Phase {0} finished OK. {1}" -f $Phase, $next) -ForegroundColor Green
    }
}
finally {
    try {
        Stop-Transcript | Out-Null
    }
    catch {
    }
    $tpDone = $script:NiusVerifyTranscriptLog
    if (-not [string]::IsNullOrWhiteSpace($tpDone)) {
        Write-Host ("[verify] Log file (open in editor): {0}" -f $tpDone) -ForegroundColor Cyan
    }
}
