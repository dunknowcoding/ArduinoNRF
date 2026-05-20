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
        $others = @(Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessId -ne $self -and $_.CommandLine -and ($_.CommandLine -match 'usb_gdbstub_bridge\.ps1') })
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
    $serial.ReadTimeout = 50
    $serial.WriteTimeout = 50
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

    $client = $listener.AcceptTcpClient()
    $client.NoDelay = $true
    $networkStream = $client.GetStream()

    Write-BridgeLog 'GDB client connected; forwarding traffic'
    Write-Host 'GDB client connected. Forwarding traffic. Press Ctrl+C to stop.'

    $buffer = New-Object byte[] 4096

    while ($client.Connected) {
        $didWork = $false

        if ($networkStream.DataAvailable) {
            $fromTcp = $networkStream.Read($buffer, 0, $buffer.Length)
            if ($fromTcp -le 0) {
                break
            }

            if ($LogTraffic) {
                Write-Host ('[tcp->serial] {0}' -f (Format-Bytes -Buffer $buffer -Length $fromTcp))
            }
            Write-BridgeLog ('gdb->mcu {0}B: {1}' -f $fromTcp, (Format-Bytes -Buffer $buffer -Length ([Math]::Min($fromTcp, 96))))

            $serial.Write($buffer, 0, $fromTcp)
            $didWork = $true
        }

        $serialBytes = $serial.BytesToRead
        if ($serialBytes -gt 0) {
            $readLength = [Math]::Min($serialBytes, $buffer.Length)
            $fromSerial = $serial.Read($buffer, 0, $readLength)
            if ($fromSerial -gt 0) {
                if ($LogTraffic) {
                    Write-Host ('[serial->tcp] {0}' -f (Format-Bytes -Buffer $buffer -Length $fromSerial))
                }
                Write-BridgeLog ('mcu->gdb {0}B: {1}' -f $fromSerial, (Format-Bytes -Buffer $buffer -Length ([Math]::Min($fromSerial, 96))))

                $networkStream.Write($buffer, 0, $fromSerial)
                $networkStream.Flush()
                $didWork = $true
            }
        }

        if (-not $didWork) {
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
