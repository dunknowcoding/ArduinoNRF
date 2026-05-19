param(
    [string]$WorkspaceRoot = "f:\Arduino\driver\ArduinoNRF",
    [string]$ConfigFile = "f:\Arduino\driver\ArduinoNRF\arduino-cli.local.yaml"
)

$matrix = @(
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840"
        Sketches = @(
            "examples\DebugPolicy",
            "examples\HardwareCapability",
            "examples\BatterySense",
            "examples\BoardSupport",
            "examples\BusParameter",
            "examples\UploadPolicy",
            "examples\BLEAdvertise",
            "examples\BLEGatt",
            "examples\SecondaryBus",
            "examples\PWMBehavior"
        )
    },
    @{
        Fqbn = "arduinonrf:nrf52:promicro_nrf52840:buildprofile=usbgdbstub"
        Sketches = @(
            "examples\DebugPolicy",
            "examples\SystemLayerInfo",
            "examples\UsbGdbStubBreakpoint",
            "examples\UsbGdbStubFault"
        )
    },
    @{
        Fqbn = "arduinonrf:nrf52:nicenano_v2"
        Sketches = @(
            "examples\DebugPolicy",
            "examples\HardwareCapability",
            "examples\BatterySense",
            "examples\BoardSupport",
            "examples\BusParameter",
            "examples\UploadPolicy",
            "examples\BLEAdvertise",
            "examples\BLEGatt",
            "examples\SecondaryBus",
            "examples\PWMBehavior"
        )
    },
    @{
        Fqbn = "arduinonrf:nrf52:supermini_nrf52840"
        Sketches = @(
            "examples\DebugPolicy",
            "examples\HardwareCapability",
            "examples\BatterySense",
            "examples\BoardSupport",
            "examples\BusParameter",
            "examples\UploadPolicy",
            "examples\BLEAdvertise",
            "examples\BLEGatt",
            "examples\SecondaryBus",
            "examples\PWMBehavior"
        )
    },
    @{
        Fqbn = "arduinonrf:nrf52:nrfmicro_nrf52840"
        Sketches = @(
            "examples\DebugPolicy",
            "examples\HardwareCapability",
            "examples\BatterySense",
            "examples\BoardSupport",
            "examples\BusParameter",
            "examples\UploadPolicy",
            "examples\BLEAdvertise",
            "examples\BLEGatt",
            "examples\SecondaryBus",
            "examples\PWMBehavior"
        )
    },
    @{
        Fqbn = "arduinonrf:nrf52:devboard_nrf52840"
        Sketches = @(
            "examples\DebugPolicy",
            "examples\HardwareCapability",
            "examples\BusParameter",
            "examples\UploadPolicy",
            "examples\BLEGatt",
            "examples\BLEAdvertise",
            "examples\SecondaryBus",
            "examples\PWMBehavior"
        )
    },
    @{
        Fqbn = "arduinonrf:nrf52:devboard_nrf52833"
        Sketches = @(
            "examples\DebugPolicy",
            "examples\HardwareCapability",
            "examples\BusParameter",
            "examples\UploadPolicy",
            "examples\SecondaryBus",
            "examples\PWMBehavior"
        )
    }
)

foreach ($entry in $matrix) {
    foreach ($sketch in $entry.Sketches) {
        $sketchPath = Join-Path $WorkspaceRoot $sketch
        Write-Host "Compiling $($entry.Fqbn) -> $sketchPath"
        arduino-cli compile --config-file $ConfigFile --fqbn $entry.Fqbn $sketchPath
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}
