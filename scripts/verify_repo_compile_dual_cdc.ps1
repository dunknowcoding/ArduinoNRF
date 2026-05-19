#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
. (Join-Path $PSScriptRoot 'arduino_cli_with_repo.ps1')
$prefix = @(Get-ArduinoCliRepoConfigArgs -EnsureJunction)
$fqbn = 'arduinonrf:nrf52:promicro_nrf52840:bootloader=promicroserialnosd,usbcdc=enabled,buildprofile=usbgdbstub'
$sketch = Join-Path (Get-Location) 'examples\MinimalUsbSmoke'
Write-Host "[verify] compile $fqbn"
& arduino-cli @prefix compile --fqbn $fqbn --clean $sketch
if ($LASTEXITCODE -ne 0) { throw 'compile failed' }
Write-Host '[verify] arduino-cli + repo sketchbook: OK'
