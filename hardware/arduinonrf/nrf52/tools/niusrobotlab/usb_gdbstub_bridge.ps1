param(
    [string]$Board = "",
    [string]$SerialPort = "",
    [int]$TcpPort = 3335,
    [int]$BaudRate = 115200,
    [string]$MatchFriendlyName = "",
    [int]$PreferMiIndex = -1,
    [switch]$PreferServiceCdc,
    [switch]$Describe,
    [switch]$ListPorts,
    [switch]$LogTraffic
)

$ErrorActionPreference = 'Stop'

$script:NiusBridgeLog = Join-Path $env:TEMP 'nius_gdbstub_bridge.log'
function Write-BridgeLog {
    param([string]$Message)
    try {
        $line = '[{0}] {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'), $Message
        Add-Content -LiteralPath $script:NiusBridgeLog -Value $line -ErrorAction SilentlyContinue
    }
    catch {
    }
}

Write-BridgeLog ('bridge start: Board={0} SerialPort={1} TcpPort={2} Baud={3} PreferServiceCdc={4} PID={5}' -f $Board, $SerialPort, $TcpPort, $BaudRate, [bool]$PreferServiceCdc, $PID)

function Clear-NiusStaleBridges {
    # A previous debug session that ended abnormally can leave another instance
    # of this bridge holding the COM port; the new session would then fail with
    # "access to the port is denied". Only one debug session is meaningful at a
    # time, so terminate any other powershell running this exact script.
    try {
        $self = $PID
        # Match only another bridge process launched with PowerShell -File.
        # A parent shell (Arduino IDE, terminal, or test runner) can contain the
        # script name in its -Command text; killing that parent orphaned this
        # bridge and made the launch appear to fail even though the TCP listener
        # survived. Exact -File matching preserves the caller and still removes
        # a stale bridge that really owns the service CDC handle.
        $bridgeFilePattern = '(?i)(?:^|\s)-File\s+(?:"[^"]*usb_gdbstub_bridge\.ps1"|\S*usb_gdbstub_bridge\.ps1)(?:\s|$)'
        $others = @(Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" -ErrorAction SilentlyContinue |
            Where-Object {
                $_.ProcessId -ne $self -and $_.CommandLine -and
                ($_.CommandLine -match $bridgeFilePattern) -and
                ($_.CommandLine -notmatch '(?i)\s-(?:Command|EncodedCommand)\b')
            })
        foreach ($p in $others) {
            Write-BridgeLog ('terminating stale bridge PID {0} (was holding the port)' -f $p.ProcessId)
            try { Stop-Process -Id $p.ProcessId -Force -ErrorAction Stop } catch { }
        }
    }
    catch {
    }
}

function Open-NiusSerialWithRetry {
    # Windows can keep a COM handle reserved for a short moment after the
    # previous owner dies, so retry briefly instead of failing the first time.
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [int]$Attempts = 10,
        [int]$DelayMs = 300
    )
    for ($i = 1; $i -le $Attempts; $i++) {
        try {
            $Serial.Open()
            return
        }
        catch {
            if ($i -ge $Attempts) {
                throw
            }
            Write-BridgeLog ('serial open attempt {0}/{1} failed ({2}); retrying in {3} ms' -f $i, $Attempts, $_.Exception.Message, $DelayMs)
            Start-Sleep -Milliseconds $DelayMs
        }
    }
}

. (Join-Path $PSScriptRoot 'usb_port_helpers.ps1')

function Get-AvailableSerialPorts {
    return [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
}

function Test-NiusBridgeYieldRequested {
    # An upload (upload.ps1) drops a request file when it needs the service COM
    # back for a 1200-bps touch. We release the port + exit so the touch can
    # reach the board (a paused debug target's halted-stub touch handler reboots
    # it into the bootloader). Uploading new firmware ends the debug session, so
    # exiting is the correct behavior; the IDE re-launches debug afterward.
    # Freshness-gated so a stale file from a killed upload can't strand a bridge.
    param([string]$Port)
    if ([string]::IsNullOrWhiteSpace($Port)) { return $false }
    if ($env:NIUS_DISABLE_BRIDGE_YIELD -eq '1') { return $false }
    $key = $Port.Trim().ToUpperInvariant()
    $f = Join-Path $env:TEMP ('nius_gdb_yield_{0}.req' -f $key)
    if (-not (Test-Path -LiteralPath $f)) { return $false }
    try {
        $age = ((Get-Date) - (Get-Item -LiteralPath $f -ErrorAction Stop).LastWriteTime).TotalSeconds
        if ($age -gt 45) { return $false }
    }
    catch {
        return $false
    }
    return $true
}

function Write-BridgeDescription {
    $availablePorts = @(Get-AvailableSerialPorts)

    [pscustomobject]@{
        transport = 'usb-cdc-gdbstub'
        board = $Board
        serialPort = $SerialPort
        tcpPort = $TcpPort
        baudRate = $BaudRate
        matchFriendlyName = $MatchFriendlyName
        preferMiIndex = $PreferMiIndex
        preferServiceCdc = [bool]$PreferServiceCdc
        implemented = $true
        status = 'tcp-serial-bridge-ready'
        availablePorts = $availablePorts
    } | ConvertTo-Json -Compress
}

function Format-Bytes {
    param(
        [byte[]]$Buffer,
        [int]$Length
    )

    if ($null -eq $Buffer -or $Length -le 0) {
        return ''
    }

    $hex = [System.BitConverter]::ToString($Buffer, 0, $Length)
    $asciiChars = New-Object System.Collections.Generic.List[char]
    for ($index = 0; $index -lt $Length; $index++) {
        $value = $Buffer[$index]
        if ($value -ge 32 -and $value -le 126) {
            $asciiChars.Add([char]$value)
        }
        else {
            $asciiChars.Add('.')
        }
    }

    $ascii = -join $asciiChars
    return ('hex={0} ascii={1}' -f $hex, $ascii)
}

if ($Describe) {
    Write-BridgeDescription
    exit 0
}

if ($ListPorts) {
    @(Get-AvailableSerialPorts) | ConvertTo-Json -Compress
    exit 0
}

if ($TcpPort -lt 1 -or $TcpPort -gt 65535) {
    throw "TCP port $TcpPort is outside the valid range 1-65535."
}

if ([string]::IsNullOrWhiteSpace($SerialPort)) {
    $availablePorts = @(Get-AvailableSerialPorts)

    if ($availablePorts.Count -eq 0) {
        throw 'No serial port was provided and no serial ports are currently available.'
    }

    if ($availablePorts.Count -eq 1) {
        $SerialPort = $availablePorts[0]
        Write-Host ("No -SerialPort given; auto-selecting the only available port: {0}" -f $SerialPort)
    }
    elseif (-not [string]::IsNullOrWhiteSpace($Board)) {
        $picked = Pick-ServiceSerialPortForGdbStub -BoardName $Board -PreferServiceCdc:$PreferServiceCdc -PreferMiIndex $PreferMiIndex -MatchFriendlyName $MatchFriendlyName
        if ($picked) {
            $SerialPort = $picked
            Write-Host ("No -SerialPort given; auto-selected board runtime port: {0}" -f $SerialPort)
        }
        else {
            throw ("No serial port was provided and automatic selection for board '{0}' is ambiguous or unsupported. Ports: {1}. Use -SerialPort COMx, or narrow with -PreferServiceCdc / -PreferMiIndex / -MatchFriendlyName." -f $Board, ($availablePorts -join ', '))
        }
    }
    else {
        throw ("No serial port was provided. Available serial ports: {0}. Re-run with -SerialPort COMx or pass -Board for filtered auto-pick." -f ($availablePorts -join ', '))
    }
}

if (-not [string]::IsNullOrWhiteSpace($Board)) {
    $portResolution = Resolve-AdafruitSerialControlPortWithBoardIdentity -SelectedPort $SerialPort -BoardName $Board
    if ($portResolution -and -not [string]::IsNullOrWhiteSpace($portResolution.Port) -and $portResolution.Port -ne $SerialPort) {
        Write-Host ("USB CDC GDB stub remap: selected {0}, using {1} ({2})" -f $SerialPort, $portResolution.Port, $portResolution.Reason)
        $SerialPort = $portResolution.Port
    }
}

$serial = $null
$listener = $null
$client = $null
$networkStream = $null

try {
    Clear-NiusStaleBridges
    Write-BridgeLog ('opening serial port {0} @ {1} baud' -f $SerialPort, $BaudRate)
    $serial = New-Object System.IO.Ports.SerialPort $SerialPort, $BaudRate, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
    $serial.Handshake = [System.IO.Ports.Handshake]::None
    # Issue bounded reads instead of polling BytesToRead. On Windows usbser the
    # managed BytesToRead property can remain zero until a read IRP is posted,
    # even though the device has already produced an RSP reply.
    $serial.ReadTimeout = 10
    # Generous write timeout: right after gdb connects, the halted stub may take
    # a few hundred ms to (re)enumerate and start servicing its CDC OUT endpoint,
    # during which a host write NAKs. A 50 ms timeout threw mid-handshake and
    # killed the bridge. The relay loop also retries on a transient timeout.
    $serial.WriteTimeout = 3000
    $serial.DtrEnable = $true
    $serial.RtsEnable = $true
    Open-NiusSerialWithRetry -Serial $serial
    Write-BridgeLog ('serial port {0} opened' -f $SerialPort)

    Write-BridgeLog ('starting TCP listener on 127.0.0.1:{0}' -f $TcpPort)
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $TcpPort)
    $listener.Start()
    Write-BridgeLog ('TCP listener up on 127.0.0.1:{0}; waiting for gdb' -f $TcpPort)

    # Emit the readiness banner cortex-debug waits for. For servertype=openocd
    # it matches the OpenOCD class initMatch():
    #   /Info\s:[^\n]*Listening on port \d+ for gdb connection/i
    # i.e. it REQUIRES the real-OpenOCD "Info : " log prefix. Without it,
    # cortex-debug never considers the server initialized and closes the
    # session after its ~10 s startup timeout. So we print the exact OpenOCD
    # format.
    #
    # CRITICAL: write via [Console]::Out and Flush(). When stdout is a pipe
    # (cortex-debug spawns us, stdout is not a console), PowerShell's Write-Host
    # is block-buffered and would not reach cortex-debug until the buffer fills
    # or we exit -- but we immediately block on AcceptTcpClient(), so the line
    # would never arrive and cortex-debug times out. An explicit flush pushes it
    # out right away.
    # NOTE: wrap each "-f" expression in its own parens. Bare
    # [Console]::Out.WriteLine("...{1}..." -f $a, $b) parses the commas as
    # separate method arguments, so -f sees only $a and String.Format throws
    # on the unfilled {1}/{2}. The extra parens make it a single string arg.
    [Console]::Out.WriteLine(("Info : Listening on port {0} for gdb connections" -f $TcpPort))
    [Console]::Out.WriteLine(("USB CDC GDB stub bridge listening on tcp://127.0.0.1:{0} and forwarding to {1} @ {2} baud" -f $TcpPort, $SerialPort, $BaudRate))
    [Console]::Out.WriteLine('Waiting for a GDB client connection...')
    [Console]::Out.Flush()

    # Wait for a gdb client, but stay responsive to an upload's yield request so
    # a debug session that is merely "armed" (bridge up, no client yet) still
    # releases the COM for an upload instead of blocking forever on accept.
    $yielded = $false
    while (-not $listener.Pending()) {
        if (Test-NiusBridgeYieldRequested -Port $SerialPort) {
            $yielded = $true
            break
        }
        Start-Sleep -Milliseconds 50
    }
    if ($yielded) {
        Write-BridgeLog 'upload yield requested before client connect; releasing serial + exiting so the upload can touch the board'
        [Console]::Out.WriteLine('Info : releasing port for firmware upload')
        [Console]::Out.Flush()
        return
    }

    $client = $listener.AcceptTcpClient()
    $client.NoDelay = $true
    $networkStream = $client.GetStream()

    Write-BridgeLog 'GDB client connected; forwarding traffic'
    Write-Host 'GDB client connected. Forwarding traffic. Press Ctrl+C to stop.'

    # A remote target must be stopped before GDB can complete its first RSP
    # handshake. The firmware treats a bare 0x03 on the dedicated service CDC
    # as the initial DebugMonitor attach as well as a later Pause request.
    # Inject it once per new TCP client before forwarding queued RSP packets;
    # otherwise GDB sends qSupported while the free-running target has not yet
    # entered the stub, and both sides wait indefinitely.
    $initialBreak = [byte[]]@(0x03)
    $serial.Write($initialBreak, 0, 1)
    Write-BridgeLog 'injected initial DebugMonitor break byte before RSP relay'
    # Keep the break in its own USB OUT transaction. Forwarding GDB's queued
    # qSupported bytes immediately can coalesce/overwrite the one-byte request
    # on clone USB firmware before the sketch's yield hook drains it. A short,
    # fixed attach grace lets DebugMonitor own the service endpoint first; the
    # device's stop reply remains buffered until the read below is posted.
    Start-Sleep -Milliseconds 100

    $buffer = New-Object byte[] 4096
    $serialReadBuffer = New-Object byte[] 4096
    # Keep one native overlapped read posted at all times. usbser can leave
    # SerialPort.BytesToRead at zero until a ReadFile request exists, and the
    # synchronous SerialPort.Read timeout is not reliable for this composite
    # CDC path. BaseStream.ReadAsync posts that request without blocking TCP.
    $serialReadTask = $serial.BaseStream.ReadAsync(
        $serialReadBuffer, 0, $serialReadBuffer.Length)

    while ($client.Connected) {
        $didWork = $false

        # TcpClient.Connected reports the state of the last socket operation,
        # not the current peer state. Detect a graceful close explicitly so a
        # finished/failed GDB client cannot leave an orphan bridge holding CDC.
        if ($client.Client.Poll(0, [System.Net.Sockets.SelectMode]::SelectRead) -and
            $client.Client.Available -eq 0) {
            Write-BridgeLog 'GDB client disconnected; ending bridge session'
            break
        }

        if ($networkStream.DataAvailable) {
            $fromTcp = $networkStream.Read($buffer, 0, $buffer.Length)
            if ($fromTcp -le 0) {
                break
            }

            if ($LogTraffic) {
                Write-Host ('[tcp->serial] {0}' -f (Format-Bytes -Buffer $buffer -Length $fromTcp))
            }
            Write-BridgeLog ('gdb->mcu {0}B: {1}' -f $fromTcp, (Format-Bytes -Buffer $buffer -Length ([Math]::Min($fromTcp, 96))))

            $writeOk = $false
            for ($attempt = 0; $attempt -lt 5 -and -not $writeOk; $attempt++) {
                try {
                    $serial.Write($buffer, 0, $fromTcp)
                    $writeOk = $true
                } catch {
                    # SerialPort.Write surfaces a write timeout as an IOException
                    # ("semaphore timeout") wrapped in a MethodInvocationException,
                    # not System.TimeoutException — so catch broadly. Right after
                    # gdb connects the halted stub may briefly NAK OUT while it
                    # (re)enumerates; retry instead of letting the bridge die.
                    Write-BridgeLog ('serial write failed (attempt {0}): {1}; retrying' -f ($attempt + 1), $_.Exception.Message)
                    Start-Sleep -Milliseconds 150
                }
            }
            if (-not $writeOk) {
                Write-BridgeLog 'serial write still timing out after retries; dropping bytes to keep bridge alive'
            }
            $didWork = $true
        }

        if ($serialReadTask.IsCompleted) {
            $fromSerial = $serialReadTask.GetAwaiter().GetResult()
            if ($fromSerial -le 0) {
                break
            }
            if ($LogTraffic) {
                Write-Host ('[serial->tcp] {0}' -f (Format-Bytes -Buffer $serialReadBuffer -Length $fromSerial))
            }
            Write-BridgeLog ('mcu->gdb {0}B: {1}' -f $fromSerial, (Format-Bytes -Buffer $serialReadBuffer -Length ([Math]::Min($fromSerial, 96))))

            $networkStream.Write($serialReadBuffer, 0, $fromSerial)
            $networkStream.Flush()
            $didWork = $true
            $serialReadTask = $serial.BaseStream.ReadAsync(
                $serialReadBuffer, 0, $serialReadBuffer.Length)
        }

        if (-not $didWork) {
            # An upload needs the COM back: stop relaying, drop the port, exit.
            # The board reboots into the new firmware, so this debug session is
            # over anyway; the IDE re-launches debug after the upload.
            if (Test-NiusBridgeYieldRequested -Port $SerialPort) {
                Write-BridgeLog 'upload yield requested during session; releasing serial + exiting for the upload'
                break
            }
            [System.Threading.Thread]::Sleep(5)
        }
    }
}
catch {
    Write-BridgeLog ('FATAL: {0}: {1}' -f $_.Exception.GetType().FullName, $_.Exception.Message)
    throw
}
finally {
    Write-BridgeLog 'bridge shutting down; releasing serial + socket'
    if ($networkStream -ne $null) {
        $networkStream.Dispose()
    }

    if ($client -ne $null) {
        $client.Close()
    }

    if ($listener -ne $null) {
        $listener.Stop()
    }

    if ($serial -ne $null -and $serial.IsOpen) {
        $serial.Close()
    }
}
