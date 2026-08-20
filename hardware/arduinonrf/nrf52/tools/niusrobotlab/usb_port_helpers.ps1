$script:NiusSerialInventoryCache = $null

# Read-only, identity-scoped USB serial discovery. Windows retains Enum\USB
# registry records after detach, so a registry record is accepted only when its
# PortName is also in SerialPort.GetPortNames(). This avoids a full PnP provider
# walk while preserving VID/PID + composite-stable-id attribution.
function Get-NiusFastUsbSerialRegistrySnapshot {
    param(
        [string]$Vid,
        [string]$ProductId,
        [string]$PreferredCompositeStableId = ''
    )

    if ([string]::IsNullOrWhiteSpace($Vid) -or [string]::IsNullOrWhiteSpace($ProductId)) {
        return [pscustomobject]@{ Available = $false; Matches = @() }
    }

    $vidToken = $Vid.Replace('0x', '').Replace('0X', '').Trim()
    $pidToken = $ProductId.Replace('0x', '').Replace('0X', '').Trim()
    if ($vidToken -notmatch '^[0-9A-Fa-f]{1,4}$' -or $pidToken -notmatch '^[0-9A-Fa-f]{1,4}$') {
        return [pscustomobject]@{ Available = $false; Matches = @() }
    }
    $vidLetters = $vidToken.PadLeft(4, '0').ToUpperInvariant()
    $pidLetters = $pidToken.PadLeft(4, '0').ToUpperInvariant()
    $compositeFamily = 'VID_{0}&PID_{1}' -f $vidLetters, $pidLetters
    $preferredStable = $PreferredCompositeStableId.Trim().ToUpperInvariant()
    $presentPorts = @{}
    foreach ($name in [System.IO.Ports.SerialPort]::GetPortNames()) {
        $presentPorts[([string]$name).Trim().ToUpperInvariant()] = $true
    }

    $usbRoot = $null
    try {
        $usbRoot = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey('SYSTEM\CurrentControlSet\Enum\USB')
        if ($null -eq $usbRoot) {
            return [pscustomobject]@{ Available = $false; Matches = @() }
        }

        $parentToStable = @{}
        $compositeRoot = $null
        try {
            $compositeRoot = $usbRoot.OpenSubKey($compositeFamily)
            if ($null -eq $compositeRoot) {
                return [pscustomobject]@{ Available = $true; Matches = @() }
            }
            foreach ($stableName in $compositeRoot.GetSubKeyNames()) {
                $normalizedStable = $stableName.Trim().ToUpperInvariant()
                if (-not [string]::IsNullOrWhiteSpace($preferredStable) -and $normalizedStable -ne $preferredStable) {
                    continue
                }
                $composite = $null
                try {
                    $composite = $compositeRoot.OpenSubKey($stableName)
                    $parentPrefix = ([string]$composite.GetValue('ParentIdPrefix', '')).Trim().ToUpperInvariant()
                    if (-not [string]::IsNullOrWhiteSpace($parentPrefix)) {
                        $parentToStable[$parentPrefix] = $normalizedStable
                    }
                }
                finally {
                    if ($null -ne $composite) { $composite.Dispose() }
                }
            }
        }
        finally {
            if ($null -ne $compositeRoot) { $compositeRoot.Dispose() }
        }

        if ($parentToStable.Count -eq 0) {
            return [pscustomobject]@{ Available = $true; Matches = @() }
        }

        $matches = New-Object 'System.Collections.Generic.List[object]'
        foreach ($interfaceFamily in $usbRoot.GetSubKeyNames()) {
            if (-not $interfaceFamily.ToUpperInvariant().StartsWith(($compositeFamily + '&MI_').ToUpperInvariant())) {
                continue
            }
            $interfaceRoot = $null
            try {
                $interfaceRoot = $usbRoot.OpenSubKey($interfaceFamily)
                foreach ($instanceName in $interfaceRoot.GetSubKeyNames()) {
                    $instanceUpper = $instanceName.Trim().ToUpperInvariant()
                    $match = [regex]::Match($instanceUpper, '^(?<parent>.+)&[0-9A-F]{4}$')
                    if (-not $match.Success) { continue }
                    $parentPrefix = $match.Groups['parent'].Value
                    if (-not $parentToStable.ContainsKey($parentPrefix)) { continue }

                    $parameters = $null
                    try {
                        $parameters = $interfaceRoot.OpenSubKey($instanceName + '\Device Parameters')
                        if ($null -eq $parameters) { continue }
                        $portName = ([string]$parameters.GetValue('PortName', '')).Trim().ToUpperInvariant()
                        if ([string]::IsNullOrWhiteSpace($portName) -or -not $presentPorts.ContainsKey($portName)) {
                            continue
                        }
                        $matches.Add([pscustomobject]@{
                                DeviceID = $portName
                                PNPDeviceID = ('USB\{0}\{1}' -f $interfaceFamily, $instanceName).ToUpperInvariant()
                                InterfaceParentPrefix = ('USB\{0}\{1}' -f $compositeFamily, $parentToStable[$parentPrefix]).ToUpperInvariant()
                                CompositeStableId = $parentToStable[$parentPrefix]
                            })
                    }
                    finally {
                        if ($null -ne $parameters) { $parameters.Dispose() }
                    }
                }
            }
            finally {
                if ($null -ne $interfaceRoot) { $interfaceRoot.Dispose() }
            }
        }
        return [pscustomobject]@{ Available = $true; Matches = $matches.ToArray() }
    }
    catch {
        return [pscustomobject]@{ Available = $false; Matches = @() }
    }
    finally {
        if ($null -ne $usbRoot) { $usbRoot.Dispose() }
    }
}

function Get-SerialPortInventory {
    # NOTE: do NOT use `Get-CimInstance Win32_SerialPort` here. That class is
    # served by the Windows Serial WMI provider, which probes every COM device;
    # on hosts with Bluetooth / modem / virtual COM ports it BLOCKS for 60-90 s
    # PER CALL (measured 90 s on this bench), and it is hit several times per
    # upload -> minutes of dead time. Every caller in this module reads only two
    # fields per port:
    #     .DeviceID    -> the COM name           (e.g. "COM3")
    #     .PNPDeviceID -> the USB instance id     (e.g. "USB\VID_239A&PID_00B4\..")
    # Both can be derived from Win32_PnPEntity when the scoped registry fast path
    # is unavailable. Derive them there and
    # never touch the Serial provider. Same cached/-Fresh shape callers expect;
    # Clear-SerialPortInventoryCache before the touch forces a fresh re-derive.
    param([switch]$Fresh)
    if ($Fresh -or $null -eq $script:NiusSerialInventoryCache) {
        $rows = New-Object System.Collections.Generic.List[object]
        foreach ($d in @(Get-PnpDeviceInventory)) {
            $com = Extract-ComPortFromPnpFriendlyName -FriendlyName ([string]$d.FriendlyName)
            if ([string]::IsNullOrWhiteSpace($com)) { continue }
            $rows.Add([pscustomobject]@{
                    DeviceID    = $com
                    PNPDeviceID = [string]$d.InstanceId
                    Name        = [string]$d.FriendlyName
                })
        }
        $script:NiusSerialInventoryCache = $rows.ToArray()
    }
    return $script:NiusSerialInventoryCache
}

function Clear-SerialPortInventoryCache {
    $script:NiusSerialInventoryCache = $null
}

$script:NiusPnpDeviceCache = $null
$script:NiusPnpCacheArmed = $false
function Set-PnpDeviceCacheArmed {
    # Get-PnpDevice -PresentOnly is a ~2-3 s full device enumeration; the
    # pre-touch identity/port checks call it 2-3x for the same VID/PID on a
    # stable (not-yet-touched) port set, so one cached scan saves several
    # seconds. Arm ONLY for that pre-touch window. MUST be disarmed before the
    # 1200 bps touch: the post-touch transition pollers also filter PnP and
    # need fresh data each iteration (a stale cache would miss the brief
    # app->bootloader detach on same-PID clones).
    param([bool]$Armed)
    $script:NiusPnpCacheArmed = $Armed
    if (-not $Armed) { $script:NiusPnpDeviceCache = $null }
}
function Get-PnpDeviceInventory {
    if ($script:NiusPnpCacheArmed -and $null -ne $script:NiusPnpDeviceCache) {
        return $script:NiusPnpDeviceCache
    }

    $snap = $null
    # FAST PATH (pre-touch window only): Get-PnpDevice -PresentOnly pays a full
    # device-presence walk (~2.5 s on this machine, independent of -Class).
    # Get-CimInstance Win32_PnPEntity returns the same present USB interfaces
    # (verified incl. composite + all MI_xx, including no-driver control
    # interfaces) in ~1.1 s, projected here to the Get-PnpDevice object shape
    # the callers expect (.InstanceId/.FriendlyName/.Class/.HardwareID/.Status).
    #
    # Scoped to $NiusPnpCacheArmed (the armed pre-touch identity/port window,
    # where the port set is stable) so the DELICATE post-touch transition
    # pollers - which run with the cache DISARMED and must catch the brief
    # app->bootloader detach on same-PID clones - keep the proven -PresentOnly
    # path untouched. Force-disable with NIUS_FORCE_PNP_LEGACY=1.
    if ($script:NiusPnpCacheArmed -and $env:NIUS_FORCE_PNP_LEGACY -ne '1') {
        try {
            $cim = @(Get-CimInstance -ClassName Win32_PnPEntity -ErrorAction Stop)
            if ($cim.Count -gt 0) {
                $snap = @($cim | Where-Object { ($null -eq $_.Present) -or $_.Present } | ForEach-Object {
                    [pscustomobject]@{
                        InstanceId   = $_.DeviceID
                        FriendlyName = $_.Name
                        Class        = $_.PNPClass
                        HardwareID   = $_.HardwareID
                        Status       = $_.Status
                    }
                })
            }
        }
        catch {
            $snap = $null
        }
    }

    if ($null -eq $snap -or $snap.Count -eq 0) {
        $snap = @(Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue)
    }
    if ($script:NiusPnpCacheArmed) { $script:NiusPnpDeviceCache = $snap }
    return $snap
}

function Get-RuntimeUsbIdentityCandidates {
    param([string]$BoardName)

    switch ($BoardName) {
        'promicro_nrf52840' {
            # The normal ProMicro clone runtime stays distinct from the serial DFU
            # bootloader: application mode is expected to enumerate as 0x00B4,
            # while 0x00B3 remains the bootloader/service upload PID. Keep
            # 0x00B3 as a fallback for special no-SoftDevice builds that opt
            # into a bootloader-PID runtime.
            return @(
                @{ Vid = '0x239A'; Pid = '0x00B4' },
                @{ Vid = '0x239A'; Pid = '0x00B3' }
            )
        }
        default {
            return @()
        }
    }
}

function Resolve-ExpectedRuntimeUsbIdentity {
    param([string]$BoardName)

    $candidates = @(Get-RuntimeUsbIdentityCandidates -BoardName $BoardName)
    if ($candidates.Count -eq 0) {
        return $null
    }
    return [pscustomobject]@{ Vid = $candidates[0].Vid; Pid = $candidates[0].Pid }
}

function Extract-ComPortFromPnpFriendlyName {
    param([string]$FriendlyName)

    if ([string]::IsNullOrWhiteSpace($FriendlyName)) {
        return $null
    }
    if ($FriendlyName -match '\((COM\d+)\)') {
        return $Matches[1]
    }
    return $null
}

function Get-UsbInterfaceParentInstancePrefix {
    param([string]$PnpInstanceId)

    if ([string]::IsNullOrWhiteSpace($PnpInstanceId)) {
        return $null
    }

    $normalized = $PnpInstanceId.Trim().ToUpperInvariant()
    if ($normalized -match '^USB\\VID_[0-9A-F]{4}&PID_[0-9A-F]{4}&MI_[0-9A-F]{2}\\(?<parent>.+&)[0-9A-F]{4}$') {
        return $matches['parent'].ToUpperInvariant()
    }

    return $null
}

function Get-UsbInterfaceParentDeviceInstanceId {
    param([string]$PnpInstanceId)

    if ([string]::IsNullOrWhiteSpace($PnpInstanceId)) {
        return $null
    }

    # Get-PnpDeviceProperty is ~2.3 s PER call on a host with many USB devices,
    # and this parent lookup is hit once per interface on every enumeration pass
    # (multiple boards x several poll loops = tens of calls = the bulk of a slow
    # upload). The InstanceId -> parent mapping is immutable for a given device
    # instance, so memoize it: the first lookup pays the cost, the rest are free.
    $key = $PnpInstanceId.Trim().ToUpperInvariant()
    # StrictMode-safe: reading an unset $script: var throws, so probe with Test-Path first.
    if (-not (Test-Path 'variable:script:NiusUsbParentCache')) { $script:NiusUsbParentCache = @{} }
    if ($script:NiusUsbParentCache.ContainsKey($key)) {
        return $script:NiusUsbParentCache[$key]
    }

    $result = $null
    try {
        $parent = Get-PnpDeviceProperty -InstanceId $PnpInstanceId -KeyName 'DEVPKEY_Device_Parent' -ErrorAction Stop | Select-Object -First 1
        if ($parent -and $parent.PSObject.Properties['Data']) {
            $result = [string]$parent.Data
        }
    }
    catch {
    }

    $script:NiusUsbParentCache[$key] = $result
    return $result
}

function Get-UsbDeviceCompositeStableId {
    param([string]$PnpInstanceId)

    if ([string]::IsNullOrWhiteSpace($PnpInstanceId)) {
        return $null
    }

    $normalized = $PnpInstanceId.Trim().ToUpperInvariant()
    if ($normalized -match '^USB\\VID_[0-9A-F]{4}&PID_[0-9A-F]{4}\\(?<tail>.+)$') {
        return $matches['tail'].ToUpperInvariant()
    }

    return $null
}

function Get-UsbInterfaceParentCompositeStableId {
    param([string]$PnpInstanceId)

    $parentInstanceId = Get-UsbInterfaceParentDeviceInstanceId -PnpInstanceId $PnpInstanceId
    if ([string]::IsNullOrWhiteSpace($parentInstanceId)) {
        return $null
    }

    return Get-UsbDeviceCompositeStableId -PnpInstanceId $parentInstanceId
}

function Get-SerialPortUsbInterfaceParentInstancePrefix {
    param(
        [string]$PortName,
        [switch]$Fresh
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        return $null
    }

    $normalizedPort = $PortName.Trim().ToUpperInvariant()
    foreach ($port in @(Get-SerialPortInventory -Fresh:$Fresh)) {
        if (([string]$port.DeviceID).Trim().ToUpperInvariant() -ne $normalizedPort) {
            continue
        }

        return Get-UsbInterfaceParentInstancePrefix -PnpInstanceId ([string]$port.PNPDeviceID)
    }

    return $null
}

function Get-SerialPortUsbParentCompositeStableId {
    param(
        [string]$PortName,
        [switch]$Fresh
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or $PortName.StartsWith('{')) {
        return $null
    }

    $normalizedPort = $PortName.Trim().ToUpperInvariant()
    foreach ($port in @(Get-SerialPortInventory -Fresh:$Fresh)) {
        if (([string]$port.DeviceID).Trim().ToUpperInvariant() -ne $normalizedPort) {
            continue
        }

        return Get-UsbInterfaceParentCompositeStableId -PnpInstanceId ([string]$port.PNPDeviceID)
    }

    return $null
}

function Get-PnpPortsMatchingBoardRuntimeUsb {
    param([string]$BoardName)

    $candidates = @(Get-RuntimeUsbIdentityCandidates -BoardName $BoardName)
    if ($candidates.Count -eq 0) {
        return @()
    }

    $devices = @(Get-PnpDeviceInventory | Where-Object { ([string]$_.Class) -eq 'Ports' })
    $matched = @()
    foreach ($d in $devices) {
        $inst = ([string]$d.InstanceId).ToUpperInvariant()
        foreach ($c in $candidates) {
            $vidLetters = $c.Vid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
            $pidLetters = $c.Pid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
            $prefix = ('USB\VID_{0}&PID_{1}' -f $vidLetters, $pidLetters).ToUpperInvariant()
            if ($inst.StartsWith($prefix)) {
                $matched += @($d)
                break
            }
        }
    }
    return $matched
}

function Pick-ServiceSerialPortForGdbStub {
    param(
        [string]$BoardName,
        [switch]$PreferServiceCdc,
        [int]$PreferMiIndex = -1,
        [string]$MatchFriendlyName = ''
    )

    $devices = @(Get-PnpPortsMatchingBoardRuntimeUsb -BoardName $BoardName)
    if ($devices.Count -eq 0) {
        return $null
    }

    if (-not [string]::IsNullOrWhiteSpace($MatchFriendlyName)) {
        $pat = ('*{0}*' -f $MatchFriendlyName.Trim())
        $devices = @($devices | Where-Object { $_.FriendlyName -like $pat })
    }
    if ($devices.Count -eq 0) {
        return $null
    }
    if ($devices.Count -eq 1) {
        return Extract-ComPortFromPnpFriendlyName -FriendlyName $devices[0].FriendlyName
    }

    $ranked = foreach ($d in $devices) {
        $inst = ([string]$d.InstanceId).ToUpperInvariant()
        $mi = 999
        if ($inst -match '&MI_([0-9A-F]{2})') {
            $mi = [Convert]::ToInt32($Matches[1], 16)
        }
        [pscustomobject]@{ Dev = $d; Mi = $mi }
    }
    $ranked = @($ranked)

    if ($PreferMiIndex -ge 0) {
        $hits = @($ranked | Where-Object { $_.Mi -eq $PreferMiIndex })
        if ($hits.Count -eq 1) {
            return Extract-ComPortFromPnpFriendlyName -FriendlyName $hits[0].Dev.FriendlyName
        }
    }

    if ($PreferServiceCdc) {
        $svc = @($ranked | Where-Object { $_.Mi -eq 0 })
        if ($svc.Count -eq 1) {
            return Extract-ComPortFromPnpFriendlyName -FriendlyName $svc[0].Dev.FriendlyName
        }
        $best = @($ranked | Sort-Object Mi | Select-Object -First 1)
        if ($best.Count -eq 1) {
            return Extract-ComPortFromPnpFriendlyName -FriendlyName $best[0].Dev.FriendlyName
        }
    }

    return $null
}

function Resolve-AdafruitSerialControlPortWithBoardIdentity {
    param(
        [string]$SelectedPort,
        [string]$BoardName
    )

    $candidates = @(Get-RuntimeUsbIdentityCandidates -BoardName $BoardName)
    if ($candidates.Count -eq 0) {
        return $null
    }

    $last = $null
    foreach ($id in $candidates) {
        $last = Resolve-AdafruitSerialControlPort -SelectedPort $SelectedPort -RuntimeVid $id.Vid -RuntimePid $id.Pid
        if (-not $last) {
            continue
        }
        if ($last.Reason -eq 'selected port not enumerated') {
            return $last
        }
        if ($last.Port -and $last.Port -ne $SelectedPort) {
            return $last
        }
        if ($last.Reason -eq 'selected port already on control/service interface') {
            return $last
        }
        if ($last.Reason -eq 'no runtime remap context') {
            return $last
        }
    }

    return $last
}

function Test-UploadUsbPidMatchesRuntimeCandidates {
    param(
        [string]$BoardName,
        [string]$UsbVid,
        [string]$UsbPid
    )

    if ([string]::IsNullOrWhiteSpace($BoardName) -or [string]::IsNullOrWhiteSpace($UsbVid) -or [string]::IsNullOrWhiteSpace($UsbPid)) {
        return $false
    }

    $uv = $UsbVid.Trim().ToUpperInvariant()
    $up = $UsbPid.Trim().ToUpperInvariant()
    foreach ($c in @(Get-RuntimeUsbIdentityCandidates -BoardName $BoardName)) {
        $cv = $c.Vid.Trim().ToUpperInvariant()
        $cp = $c.Pid.Trim().ToUpperInvariant()
        if ($uv -eq $cv -and $up -eq $cp) {
            return $true
        }
    }
    return $false
}

function Get-NiusBoardRuntimeSerialPortsRankedByMi {
    param([string]$BoardName)

    $rows = New-Object System.Collections.Generic.List[object]
    foreach ($sp in @(Get-SerialPortInventory)) {
        $pnp = ([string]$sp.PNPDeviceID).ToUpperInvariant()
        foreach ($id in @(Get-RuntimeUsbIdentityCandidates -BoardName $BoardName)) {
            $vidLetters = $id.Vid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
            $pidLetters = $id.Pid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
            $needle = ('USB\VID_{0}&PID_{1}' -f $vidLetters, $pidLetters).ToUpperInvariant()
            if (-not $pnp.StartsWith($needle)) {
                continue
            }
            $mi = 999
            if ($pnp -match '&MI_([0-9A-F]{2})') {
                $mi = [Convert]::ToInt32($Matches[1], 16)
            }
            $rows.Add([pscustomobject]@{
                    Com = [string]$sp.DeviceID.Trim()
                    Mi = $mi
                    PNPDeviceID = $sp.PNPDeviceID
                })
            break
        }
    }
    return @($rows.ToArray() | Sort-Object Mi)
}

function Get-NiusServiceComForBoard {
    param([string]$BoardName)

    $ranked = @(Get-NiusBoardRuntimeSerialPortsRankedByMi -BoardName $BoardName)
    if ($ranked.Count -eq 0) {
        return $null
    }
    return [string]$ranked[0].Com
}

function Get-NiusUserComForBoardIfDual {
    param([string]$BoardName)

    $ranked = @(Get-NiusBoardRuntimeSerialPortsRankedByMi -BoardName $BoardName)
    if ($ranked.Count -lt 2) {
        return $null
    }
    return [string]$ranked[$ranked.Count - 1].Com
}

function Resolve-AdafruitSerialControlPort {
    param(
        [string]$SelectedPort,
        [string]$RuntimeVid = '',
        [string]$RuntimePid = ''
    )

    if ([string]::IsNullOrWhiteSpace($SelectedPort) -or
        $SelectedPort.StartsWith('{') -or
        [string]::IsNullOrWhiteSpace($RuntimeVid) -or
        [string]::IsNullOrWhiteSpace($RuntimePid)) {
        return [pscustomobject]@{
            Port = $SelectedPort
            Reason = 'no runtime remap context'
        }
    }

    $ports = @(Get-SerialPortInventory)
    if ($ports.Count -eq 0) {
        return [pscustomobject]@{
            Port = $SelectedPort
            Reason = 'no serial inventory'
        }
    }

    $normalizedSelectedPort = $SelectedPort.Trim().ToUpperInvariant()
    $selected = @($ports | Where-Object {
        ([string]$_.DeviceID).Trim().ToUpperInvariant() -eq $normalizedSelectedPort
    } | Select-Object -First 1)
    if (-not $selected) {
        return [pscustomobject]@{
            Port = $SelectedPort
            Reason = 'selected port not enumerated'
        }
    }

    $runtimeVidLetters = $RuntimeVid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $runtimePidLetters = $RuntimePid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $runtimeNeedle = ('USB\VID_{0}&PID_{1}' -f $runtimeVidLetters, $runtimePidLetters).ToUpperInvariant()
    $selectedPnpId = ([string]$selected.PNPDeviceID).ToUpperInvariant()
    if (-not $selectedPnpId.StartsWith($runtimeNeedle)) {
        return [pscustomobject]@{
            Port = $SelectedPort
            Reason = 'selected port is not runtime USB CDC'
        }
    }

    if ($selectedPnpId -notlike '*&MI_02\*') {
        return [pscustomobject]@{
            Port = $SelectedPort
            Reason = 'selected port already on control/service interface'
        }
    }

    $siblingCandidates = @($ports | Where-Object {
        $pnpId = ([string]$_.PNPDeviceID).ToUpperInvariant()
        $pnpId.StartsWith($runtimeNeedle) -and $pnpId -like '*&MI_00\*'
    })
    if ($siblingCandidates.Count -eq 1) {
        return [pscustomobject]@{
            Port = [string]$siblingCandidates[0].DeviceID
            Reason = ('runtime user CDC {0} remapped to sibling service CDC {1}' -f $SelectedPort, [string]$siblingCandidates[0].DeviceID)
        }
    }

    if ($selectedPnpId -match '^USB\\VID_[0-9A-F]{4}&PID_[0-9A-F]{4}&MI_[0-9A-F]{2}\\(?<parent>.+&)[0-9A-F]{4}$') {
        $parentPrefix = $matches['parent'].ToUpperInvariant()
        $parentMatch = @($siblingCandidates | Where-Object {
            ([string]$_.PNPDeviceID).ToUpperInvariant() -like ('*\' + $parentPrefix + '*')
        } | Select-Object -First 1)
        if ($parentMatch) {
            return [pscustomobject]@{
                Port = [string]$parentMatch.DeviceID
                Reason = ('runtime user CDC {0} remapped to sibling service CDC {1} via parent instance match' -f $SelectedPort, [string]$parentMatch.DeviceID)
            }
        }
    }

    return [pscustomobject]@{
        Port = $SelectedPort
        Reason = 'runtime sibling service CDC not found'
    }
}

function Resolve-AdafruitBootloaderControlPort {
    param(
        [string]$SelectedPort,
        [string]$CurrentPort = '',
        [string]$BootloaderVid = '',
        [string]$BootloaderPid = '',
        [string]$PreferredCompositeStableId = '',
        [string]$InterfaceParentPrefix = '',
        [switch]$Fresh
    )

    $fallbackPort = if (-not [string]::IsNullOrWhiteSpace($CurrentPort)) { $CurrentPort } else { $SelectedPort }
    if ([string]::IsNullOrWhiteSpace($fallbackPort) -or
        $fallbackPort.StartsWith('{') -or
        [string]::IsNullOrWhiteSpace($BootloaderVid) -or
        [string]::IsNullOrWhiteSpace($BootloaderPid)) {
        return [pscustomobject]@{
            Port = $fallbackPort
            Reason = 'no bootloader remap context'
        }
    }

    # Prefer the exact VID/PID registry snapshot. It is scoped to currently
    # present COM names and preserves the composite stable identity without a
    # full Get-PnpDevice enumeration while Windows is retiring runtime nodes.
    $fastSnapshot = Get-NiusFastUsbSerialRegistrySnapshot `
        -Vid $BootloaderVid `
        -ProductId $BootloaderPid `
        -PreferredCompositeStableId $PreferredCompositeStableId
    if ($fastSnapshot.Available) {
        $ports = @($fastSnapshot.Matches)
    }
    else {
        $ports = @(Get-SerialPortInventory -Fresh:$Fresh)
    }
    if ($ports.Count -eq 0) {
        return [pscustomobject]@{
            Port = $fallbackPort
            Reason = 'no serial inventory'
        }
    }

    $bootVidLetters = $BootloaderVid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $bootPidLetters = $BootloaderPid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $bootNeedle = ('USB\VID_{0}&PID_{1}' -f $bootVidLetters, $bootPidLetters).ToUpperInvariant()
    $fallbackPortNormalized = $fallbackPort.Trim().ToUpperInvariant()

    $candidates = @($ports | Where-Object {
        $pnpId = ([string]$_.PNPDeviceID).ToUpperInvariant()
        $pnpId.StartsWith($bootNeedle) -and $pnpId -like '*&MI_00\*'
    })
    if ($candidates.Count -eq 0) {
        return [pscustomobject]@{
            Port = $fallbackPort
            Reason = 'bootloader service CDC not enumerated'
        }
    }

    $exact = @($candidates | Where-Object {
        ([string]$_.DeviceID).Trim().ToUpperInvariant() -eq $fallbackPortNormalized
    } | Select-Object -First 1)
    if ($exact) {
        return [pscustomobject]@{
            Port = [string]$exact[0].DeviceID
            Reason = 'current port already on bootloader service interface'
        }
    }

    $preferredStable = $PreferredCompositeStableId.Trim().ToUpperInvariant()
    if (-not [string]::IsNullOrWhiteSpace($preferredStable)) {
        $stableMatches = @($candidates | Where-Object {
            (Get-UsbInterfaceParentCompositeStableId -PnpInstanceId ([string]$_.PNPDeviceID)) -eq $preferredStable
        })
        if ($stableMatches.Count -eq 1) {
            return [pscustomobject]@{
                Port = [string]$stableMatches[0].DeviceID
                Reason = ('bootloader service CDC matched runtime composite identity {0}' -f $preferredStable)
            }
        }
    }

    $normalizedParentPrefix = $InterfaceParentPrefix.Trim().ToUpperInvariant()
    if (-not [string]::IsNullOrWhiteSpace($normalizedParentPrefix)) {
        $parentMatches = @($candidates | Where-Object {
            (Get-UsbInterfaceParentInstancePrefix -PnpInstanceId ([string]$_.PNPDeviceID)) -eq $normalizedParentPrefix
        })
        if ($parentMatches.Count -eq 1) {
            return [pscustomobject]@{
                Port = [string]$parentMatches[0].DeviceID
                Reason = ('bootloader service CDC matched runtime interface parent prefix {0}' -f $normalizedParentPrefix)
            }
        }
    }

    if ($candidates.Count -eq 1) {
        return [pscustomobject]@{
            Port = [string]$candidates[0].DeviceID
            Reason = 'single bootloader service CDC candidate'
        }
    }

    return [pscustomobject]@{
        Port = $fallbackPort
        Reason = 'bootloader service CDC ambiguous'
    }
}

function Test-SerialPortMatchesUsbIdentity {
    param(
        [string]$PortName,
        [string]$Vid = '',
        [string]$ProductId = ''
    )

    if ([string]::IsNullOrWhiteSpace($PortName) -or
        $PortName.StartsWith('{') -or
        [string]::IsNullOrWhiteSpace($Vid) -or
        [string]::IsNullOrWhiteSpace($ProductId)) {
        return $false
    }

    $normalizedPort = $PortName.Trim().ToUpperInvariant()
    $fastSnapshot = Get-NiusFastUsbSerialRegistrySnapshot -Vid $Vid -ProductId $ProductId
    if ($fastSnapshot.Available) {
        return @($fastSnapshot.Matches | Where-Object {
                ([string]$_.DeviceID).Trim().ToUpperInvariant() -eq $normalizedPort
            }).Count -gt 0
    }

    $vidLetters = $Vid.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $pidLetters = $ProductId.Replace('0x', '').Replace('0X', '').Trim().ToUpperInvariant()
    $needle = ('USB\VID_{0}&PID_{1}' -f $vidLetters, $pidLetters).ToUpperInvariant()

    foreach ($port in @(Get-SerialPortInventory)) {
        if (([string]$port.DeviceID).Trim().ToUpperInvariant() -ne $normalizedPort) {
            continue
        }
        return ([string]$port.PNPDeviceID).ToUpperInvariant().StartsWith($needle)
    }

    return $false
}
