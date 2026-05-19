param(
    [string]$Port,
    [string]$DriveLetter,
    [switch]$AsJson
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

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

function Get-BoardCatalog {
    return @(
        [pscustomobject]@{
            PackageVariant = 'nicenano_v2'
            BoardFamily = 'promicro-compatible'
            CandidateSilkscreen = 'nice!nano v2 or compatible'
            MatchType = 'uf2-model'
            MatchValue = 'nice!nano'
            PinEvidence = 'reference-backed nicenano_v2 variant + docs'
            Notes = 'Bootloader identity and UF2 volume directly match the nice!nano family.'
            KeyPinFunctions = [ordered]@{
                serial = 'RX=P0.08, TX=P1.08'
                wire = 'SDA=P0.17, SCL=P0.20'
                spi = 'MISO=P0.15, MOSI=P0.13, SCK=P0.24'
                wire1 = 'SDA=P0.06, SCL=P0.29'
                spi1 = 'MISO=P0.22, MOSI=P0.20, SCK=P0.17'
                battery = 'VDDHDIV5'
                extVcc = 'P0.10'
            }
        },
        [pscustomobject]@{
            PackageVariant = 'promicro_nrf52840'
            BoardFamily = 'promicro-compatible'
            CandidateSilkscreen = 'AliExpress ProMicro nRF52840 or compatible'
            MatchType = 'usb-vidpid'
            MatchValue = '1915:5281'
            PinEvidence = 'partial promicro_nrf52840 variant + clone-image-backed notes'
            Notes = 'This package model is a generic ProMicro-class clone profile, not a vendor-locked pinout guarantee.'
            KeyPinFunctions = [ordered]@{
                serial = 'RX=P0.08, TX=P1.08'
                wire = 'SDA=P0.17, SCL=P0.20'
                spi = 'MISO=P0.15, MOSI=P0.13, SCK=P0.24'
                wire1 = 'unassigned in current variant'
                spi1 = 'unassigned in current variant'
                battery = 'ADC pin D29'
                extVcc = 'not modeled'
            }
        },
        [pscustomobject]@{
            PackageVariant = 'supermini_nrf52840'
            BoardFamily = 'promicro-compatible'
            CandidateSilkscreen = 'SuperMini nRF52840 or compatible'
            MatchType = 'usb-vidpid'
            MatchValue = '1915:5287'
            PinEvidence = 'reference-core-backed secondary-bus mapping + community-image-backed notes'
            Notes = 'Clone-to-clone board labels vary; secondary-bus mapping is modeled from reference material.'
            KeyPinFunctions = [ordered]@{
                serial = 'RX=P0.08, TX=P1.08'
                wire = 'SDA=P0.17, SCL=P0.20'
                spi = 'MISO=P0.15, MOSI=P0.13, SCK=P0.24'
                wire1 = 'SDA=P0.06, SCL=P0.29'
                spi1 = 'MISO=P0.22, MOSI=P0.20, SCK=P0.17'
                battery = 'VDDHDIV5'
                extVcc = 'P0.10'
            }
        },
        [pscustomobject]@{
            PackageVariant = 'nrfmicro_nrf52840'
            BoardFamily = 'promicro-compatible'
            CandidateSilkscreen = 'nRFMicro nRF52840 or compatible'
            MatchType = 'usb-vidpid'
            MatchValue = '1915:5288'
            PinEvidence = 'reference-core-backed variant + community-image-backed notes'
            Notes = 'This is the best-fit package variant when the board advertises the nRFMicro bootloader identity.'
            KeyPinFunctions = [ordered]@{
                serial = 'RX=P0.08, TX=P1.08'
                wire = 'SDA=P0.17, SCL=P0.20'
                spi = 'MISO=P0.15, MOSI=P0.13, SCK=P0.24'
                wire1 = 'SDA=P0.06, SCL=P0.29'
                spi1 = 'MISO=P0.22, MOSI=P0.20, SCK=P0.17'
                battery = 'ADC pin D29'
                extVcc = 'P0.10'
            }
        }
    )
}

function Get-PresentUsbInterfaces {
    param([string]$PortName)

    $devices = @()
    if (-not [string]::IsNullOrWhiteSpace($PortName)) {
        $devices = @(Get-PnpDevice -PresentOnly | Where-Object { $_.FriendlyName -eq ("USB Serial Device ({0})" -f $PortName) -or $_.FriendlyName -like ("*({0})" -f $PortName) })
    }

    if ($devices.Count -eq 0) {
        $devices = @(Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match 'VID_[0-9A-F]{4}&PID_[0-9A-F]{4}' })
    }

    return $devices
}

function Get-PrimaryUsbIdentity {
    param([object[]]$Devices)

    $portDevice = $Devices | Where-Object { $_.Class -eq 'Ports' } | Select-Object -First 1
    $identitySource = $portDevice
    if (-not $identitySource) {
        $identitySource = $Devices | Select-Object -First 1
    }

    if (-not $identitySource) {
        return $null
    }

    if ($identitySource.InstanceId -match 'VID_(?<vid>[0-9A-F]{4})&PID_(?<pid>[0-9A-F]{4})') {
        return [pscustomobject]@{
            UsbVid = $matches['vid'].ToLowerInvariant()
            UsbPid = $matches['pid'].ToLowerInvariant()
            InstanceId = $identitySource.InstanceId
            FriendlyName = $identitySource.FriendlyName
        }
    }

    return $null
}

function Resolve-Uf2Root {
    param([string]$RequestedDriveLetter)

    if (-not [string]::IsNullOrWhiteSpace($RequestedDriveLetter)) {
        $candidate = ('{0}:\' -f $RequestedDriveLetter.TrimEnd(':'))
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $volumes = Get-Volume | Where-Object { $_.DriveType -eq 'Removable' -or $_.FileSystemLabel -match 'UF2|NANO|NRF|BOOT' }
    foreach ($volume in $volumes) {
        if ($volume.DriveLetter) {
            $candidate = ('{0}:\' -f $volume.DriveLetter)
            if (Test-Path -LiteralPath (Join-Path $candidate 'INFO_UF2.TXT')) {
                return $candidate
            }
        }
    }

    return $null
}

function Resolve-CandidateBoard {
    param(
        [object]$CatalogEntry,
        [object]$UsbIdentity,
        [object]$Uf2Info
    )

    if ($CatalogEntry.MatchType -eq 'uf2-model' -and $Uf2Info) {
        if ($Uf2Info.PSObject.Properties.Name -contains 'Model' -and $Uf2Info.Model -eq $CatalogEntry.MatchValue) {
            return $true
        }
    }

    if ($CatalogEntry.MatchType -eq 'usb-vidpid' -and $UsbIdentity) {
        $usbId = ('{0}:{1}' -f $UsbIdentity.UsbVid, $UsbIdentity.UsbPid)
        if ($usbId -eq $CatalogEntry.MatchValue) {
            return $true
        }
    }

    return $false
}

$usbDevices = @(Get-PresentUsbInterfaces -PortName $Port)
$usbIdentity = Get-PrimaryUsbIdentity -Devices $usbDevices
$uf2Root = Resolve-Uf2Root -RequestedDriveLetter $DriveLetter
$uf2Info = if ($uf2Root) { Get-Uf2Info -RootPath $uf2Root } else { $null }
$catalog = Get-BoardCatalog
$matchedBoards = @($catalog | Where-Object { Resolve-CandidateBoard -CatalogEntry $_ -UsbIdentity $usbIdentity -Uf2Info $uf2Info })

$bootloaderFamily = 'unknown'
$bootloaderEvidence = 'none'
if ($uf2Info) {
    $bootloaderFamily = 'uf2'
    $bootloaderEvidence = 'INFO_UF2.TXT'
} elseif ($usbIdentity -and ('{0}:{1}' -f $usbIdentity.UsbVid, $usbIdentity.UsbPid) -like '1915:*') {
    $bootloaderFamily = 'nordic-dfu'
    $bootloaderEvidence = 'USB VID/PID'
}

$report = [pscustomobject]@{
    Port = if ([string]::IsNullOrWhiteSpace($Port)) { $null } else { $Port }
    UsbVid = if ($usbIdentity) { $usbIdentity.UsbVid } else { $null }
    UsbPid = if ($usbIdentity) { $usbIdentity.UsbPid } else { $null }
    UsbFriendlyName = if ($usbIdentity) { $usbIdentity.FriendlyName } else { $null }
    BootloaderFamily = $bootloaderFamily
    BootloaderEvidence = $bootloaderEvidence
    Uf2Drive = $uf2Root
    Uf2Model = if ($uf2Info -and ($uf2Info.PSObject.Properties.Name -contains 'Model')) { $uf2Info.Model } else { $null }
    Uf2BoardId = if ($uf2Info -and ($uf2Info.PSObject.Properties.Name -contains 'Board-ID')) { $uf2Info.'Board-ID' } else { $null }
    Uf2SoftDevice = if ($uf2Info -and ($uf2Info.PSObject.Properties.Name -contains 'SoftDevice')) { $uf2Info.SoftDevice } else { $null }
    CandidateBoards = @($matchedBoards | ForEach-Object {
        [pscustomobject]@{
            packageVariant = $_.PackageVariant
            boardFamily = $_.BoardFamily
            candidateSilkscreen = $_.CandidateSilkscreen
            pinEvidence = $_.PinEvidence
            notes = $_.Notes
            keyPinFunctions = [pscustomobject]$_.KeyPinFunctions
        }
    })
    RawUsbInterfaces = @($usbDevices | ForEach-Object {
        [pscustomobject]@{
            class = $_.Class
            friendlyName = $_.FriendlyName
            instanceId = $_.InstanceId
        }
    })
}

if ($AsJson) {
    $report | ConvertTo-Json -Depth 6
    exit 0
}

Write-Host 'Board Identity Report'
Write-Host '====================='
Write-Host ('Port: {0}' -f ($(if ($report.Port) { $report.Port } else { 'n/a' })))
Write-Host ('USB: {0}:{1} {2}' -f ($(if ($report.UsbVid) { $report.UsbVid } else { 'unknown' }), $(if ($report.UsbPid) { $report.UsbPid } else { 'unknown' }), $(if ($report.UsbFriendlyName) { "({0})" -f $report.UsbFriendlyName } else { '' })))
Write-Host ('Bootloader: {0} via {1}' -f $report.BootloaderFamily, $report.BootloaderEvidence)
if ($report.Uf2Drive) {
    Write-Host ('UF2 drive: {0}' -f $report.Uf2Drive)
}
if ($report.Uf2Model) {
    Write-Host ('UF2 model: {0}' -f $report.Uf2Model)
}
if ($report.Uf2BoardId) {
    Write-Host ('UF2 board-id: {0}' -f $report.Uf2BoardId)
}
if ($report.Uf2SoftDevice) {
    Write-Host ('UF2 softdevice: {0}' -f $report.Uf2SoftDevice)
}

if ($report.CandidateBoards.Count -gt 0) {
    Write-Host ''
    Write-Host 'Candidate package variants:'
    foreach ($candidate in $report.CandidateBoards) {
        Write-Host ('- {0}: silkscreen "{1}"; pin evidence: {2}' -f $candidate.packageVariant, $candidate.candidateSilkscreen, $candidate.pinEvidence)
        Write-Host ('  note: {0}' -f $candidate.notes)
        Write-Host ('  pins: Serial={0}; Wire={1}; SPI={2}; Wire1={3}; SPI1={4}; Battery={5}; EXT_VCC={6}' -f $candidate.keyPinFunctions.serial, $candidate.keyPinFunctions.wire, $candidate.keyPinFunctions.spi, $candidate.keyPinFunctions.wire1, $candidate.keyPinFunctions.spi1, $candidate.keyPinFunctions.battery, $candidate.keyPinFunctions.extVcc)
    }
} else {
    Write-Host ''
    Write-Host 'Candidate package variants: none matched current evidence'
}