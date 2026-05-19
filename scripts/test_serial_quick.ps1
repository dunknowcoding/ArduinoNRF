param([string]$ComPort = 'COM3')
try {
    $s = New-Object System.IO.Ports.SerialPort($ComPort, 115200, 'None', 8, 'One')
    $s.Open()
    $s.Dispose()
    Write-Host "OPEN_OK $ComPort 115200"
    exit 0
} catch {
    Write-Host ("OPEN_FAIL {0}: {1}" -f $ComPort, $_.Exception.Message)
    exit 1
}
