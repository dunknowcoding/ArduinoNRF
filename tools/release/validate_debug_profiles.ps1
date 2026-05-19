param(
    [string]$WorkspaceRoot = "f:\Arduino\driver\ArduinoNRF",
    [string]$ConfigFile = "f:\Arduino\driver\ArduinoNRF\arduino-cli.local.yaml"
)

$matrix = @(
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840:buildprofile=debug,uploadmode=usb"
        Sketch = "examples\SystemLayerInfo"
    },
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840:buildprofile=debug,uploadmode=usb"
        Sketch = "examples\DebugPolicy"
    },
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840:buildprofile=debug,uploadmode=swd"
        Sketch = "examples\SystemLayerInfo"
    },
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840:buildprofile=debug,uploadmode=swd"
        Sketch = "examples\DebugPolicy"
    },
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840:buildprofile=usbgdbstub"
        Sketch = "examples\SystemLayerInfo"
    },
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840:buildprofile=usbgdbstub"
        Sketch = "examples\DebugPolicy"
    },
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840:buildprofile=usbgdbstub"
        Sketch = "examples\UsbGdbStubBreakpoint"
    },
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840:buildprofile=usbgdbstub"
        Sketch = "examples\UsbGdbStubFault"
    },
    @{
        Fqbn = "arduinonrf:nrf52:devboard_nrf52840:buildprofile=debug,uploadmode=swd"
        Sketch = "examples\SystemLayerInfo"
    },
    @{
        Fqbn = "arduinonrf:nrf52:devboard_nrf52833:buildprofile=debug"
        Sketch = "examples\SystemLayerInfo"
    },
    @{
        Fqbn = "arduinonrf:nrf52:devboard_nrf52833:buildprofile=debug"
        Sketch = "examples\DebugPolicy"
    }
)

foreach ($entry in $matrix) {
    $sketchPath = Join-Path $WorkspaceRoot $entry.Sketch
    Write-Host "Compiling debug profile $($entry.Fqbn) -> $sketchPath"
    arduino-cli compile --config-file $ConfigFile --fqbn $entry.Fqbn $sketchPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}