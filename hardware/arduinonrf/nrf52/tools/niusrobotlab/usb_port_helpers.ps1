$script:NiusSerialInventoryCache = $null
function Get-SerialPortInventory {
    # Win32_SerialPort is a ~1-3 s WMI query. The pre-touch port-resolution
    # helpers call this several times in a row on a stable (not-yet-touched)
    # port set, so cache the result and reuse it. All hot detach/settle loops
    # use the instant [SerialPort]::GetPortNames() instead, never this, so the
    # cache can't go stale under them. Clear-SerialPortInventoryCache is called
    # right before the 1200 bps touch so any later call re-queries fresh.
    param([switch]$Fresh)
    if ($Fresh -or $null -eq $script:NiusSerialInventoryCache) {
        $script:NiusSerialInventoryCache = @(Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue)
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
    $snap = @(Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue)
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

    if ($selectedPnpId -match '^USB\\VID_[0-9A-F]{4}&PID_[0-9A-F]{4}&MI_[0-9A-F]{2}\\(?<parent>.+&0&)[0-9A-F]{4}$') {
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
