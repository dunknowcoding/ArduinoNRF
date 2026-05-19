#Requires -Version 5.1
<#
.SYNOPSIS
  Step H — Host port helpers + GDB stub bridge smoke (ProMicro / promicro_nrf52840).

.DESCRIPTION
  Run from repository root (or any cwd — script resolves repo).
  - Prints PID candidates, PreferServiceCdc COM pick, remap probe per COM.
  - usb_gdbstub_bridge -Describe JSON.
  Optional -TcpSmoke: starts bridge in background, connects loopback TCP once, stops (proves listener + COM open).

.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File scripts\hw_verify_step_host_bridge.ps1
  powershell -NoProfile -ExecutionPolicy Bypass -File scripts\hw_verify_step_host_bridge.ps1 -TcpSmoke -TcpPort 3337
#>
param(
    [switch]$TcpSmoke,
    [ValidateRange(1, 65535)]
    [int]$TcpPort = 3337
)

$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $Repo
. (Join-Path $Repo 'hardware/arduinonrf/nrf52/tools/niusrobotlab/usb_port_helpers.ps1')

Write-Host '=== H1 Runtime USB identity candidates (promicro_nrf52840) ===' -ForegroundColor Cyan
$cands = @(Get-RuntimeUsbIdentityCandidates -BoardName 'promicro_nrf52840')
$idx = 0
foreach ($c in $cands) {
    Write-Host ("  [{0}] VID={1} PID={2}" -f $idx, $c.Vid, $c.Pid)
    $idx++
}

Write-Host '=== H2 Pick-ServiceSerialPortForGdbStub -PreferServiceCdc ===' -ForegroundColor Cyan
$pick = Pick-ServiceSerialPortForGdbStub -BoardName 'promicro_nrf52840' -PreferServiceCdc
Write-Host ("Selected: {0}" -f $(if ($pick) { $pick } else { '(null — plug board or check VID/PID)' }))

$ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
Write-Host '=== H3 SerialPort.GetPortNames ===' -ForegroundColor Cyan
Write-Host $(if ($ports.Count) { $ports -join ', ' } else { '(none)' })

Write-Host '=== H4 Resolve-AdafruitSerialControlPortWithBoardIdentity (per COM) ===' -ForegroundColor Cyan
foreach ($name in $ports) {
    $r = Resolve-AdafruitSerialControlPortWithBoardIdentity -SelectedPort $name -BoardName 'promicro_nrf52840'
    if ($r) {
        Write-Host ("  {0} -> Port={1} Reason={2}" -f $name, $r.Port, $r.Reason)
    }
}

Write-Host '=== H5 usb_gdbstub_bridge -Describe ===' -ForegroundColor Cyan
$bp = Join-Path $Repo 'hardware/arduinonrf/nrf52/tools/niusrobotlab/usb_gdbstub_bridge.ps1'
& powershell -NoProfile -ExecutionPolicy Bypass -File $bp -Describe -Board 'promicro_nrf52840' -TcpPort 3335

if ($TcpSmoke) {
    if (-not $pick) {
        Write-Host '[TcpSmoke] skipped — no COM from H2' -ForegroundColor Yellow
    }
    else {
        Write-Host ("=== H6 TCP smoke localhost:{0} -> bridge -> {1} ===" -f $TcpPort, $pick) -ForegroundColor Cyan
        $psiArgs = @(
            '-NoProfile',
            '-ExecutionPolicy', 'Bypass',
            '-File', $bp,
            '-SerialPort', $pick,
            '-TcpPort', ([string]$TcpPort)
        )
        $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList $psiArgs -PassThru -WindowStyle Hidden

        $ok = $false
        $deadline = [datetime]::UtcNow.AddSeconds(22)
        while ([datetime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 300
            if ($proc.HasExited) {
                break
            }
            try {
                $tcp = New-Object System.Net.Sockets.TcpClient
                $tcp.Connect([System.Net.IPAddress]::Loopback, $TcpPort)
                $tcp.Close()
                $ok = $true
                break
            }
            catch {
            }
        }

        if (-not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        }

        if ($ok) {
            Write-Host '[TcpSmoke] CONNECT OK (loopback TCP reached bridge listener)' -ForegroundColor Green
        }
        else {
            Write-Host ('[TcpSmoke] FAILED — bridge process exit={0}' -f $proc.ExitCode) -ForegroundColor Red
            Write-Host '  Common causes: COM port busy (close Arduino Serial Monitor / PuTTY / screen); pick another -TcpPort; or bridge threw before Listen.' -ForegroundColor Yellow
        }
    }
}

Write-Host '=== Step H host port / bridge verification done ===' -ForegroundColor Green
