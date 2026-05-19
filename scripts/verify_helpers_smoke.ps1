#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
. (Join-Path 'hardware/arduinonrf/nrf52/tools/niusrobotlab' 'usb_port_helpers.ps1')

$c = @(Get-RuntimeUsbIdentityCandidates -BoardName 'promicro_nrf52840')
if ($c.Count -lt 2) { throw 'expected >=2 PID candidates' }
if ($c[0].Pid -ne '0x00B3') { throw ('primary PID should be 0x00B3, got ' + $c[0].Pid) }

$exp = Resolve-ExpectedRuntimeUsbIdentity -BoardName 'promicro_nrf52840'
if (-not $exp -or $exp.Pid -ne '0x00B3') { throw 'Resolve-ExpectedRuntimeUsbIdentity primary PID mismatch' }

$picked = Pick-ServiceSerialPortForGdbStub -BoardName 'promicro_nrf52840' -PreferServiceCdc -PreferMiIndex -1 -MatchFriendlyName ''
Write-Host ('Pick-ServiceSerialPortForGdbStub (promicro, PreferServiceCdc): ' + $(if ($picked) { $picked } else { '(null — ambiguous or no matching PnP Ports)' }))

$bridgePs1 = Join-Path 'hardware/arduinonrf/nrf52/tools/niusrobotlab' 'usb_gdbstub_bridge.ps1'
$describe = & powershell -NoProfile -ExecutionPolicy Bypass -File $bridgePs1 -Describe -Board 'promicro_nrf52840' -TcpPort 3335 2>&1
if ($LASTEXITCODE -ne 0) { throw 'usb_gdbstub_bridge -Describe failed' }
Write-Host 'usb_gdbstub_bridge -Describe OK'
Write-Host $describe

Write-Host 'usb_port_helpers smoke: OK'
