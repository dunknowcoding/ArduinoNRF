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
    $serial = New-Object System.IO.Ports.SerialPort $SerialPort, $BaudRate, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
    $serial.Handshake = [System.IO.Ports.Handshake]::None
    $serial.ReadTimeout = 50
    $serial.WriteTimeout = 50
    $serial.DtrEnable = $true
    $serial.RtsEnable = $true
    $serial.Open()

    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $TcpPort)
    $listener.Start()

    Write-Host ("USB CDC GDB stub bridge listening on tcp://127.0.0.1:{0} and forwarding to {1} @ {2} baud" -f $TcpPort, $SerialPort, $BaudRate)
    Write-Host 'Waiting for a GDB client connection...'

    $client = $listener.AcceptTcpClient()
    $client.NoDelay = $true
    $networkStream = $client.GetStream()

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
finally {
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
