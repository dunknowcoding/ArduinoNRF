#Requires -Version 5.1
<#
.SYNOPSIS
  Poll Windows PnP for USB devices mentioning VID 239A (Adafruit / clone nRF52)
  and append lines to a log file. Use while resetting or uploading when Device
  Manager flashes too fast to read.

.EXAMPLE
  .\scripts\usb_vid_watch.ps1 -Seconds 120
  .\scripts\usb_vid_watch.ps1 -Seconds 300 -LogPath "$env:USERPROFILE\Desktop\nrf_usb.log"
#>
param(
    [int]$Seconds = 120,
    [string]$LogPath = ''
)

if ([string]::IsNullOrWhiteSpace($LogPath)) {
    $LogPath = Join-Path $env:TEMP 'ArduinoNRF_usb_vid_watch.log'
}

$start = Get-Date
"===== usb_vid_watch start $($start.ToString('o')) duration=${Seconds}s log=$LogPath =====" | Out-File -FilePath $LogPath -Append -Encoding utf8

$deadline = $start.AddSeconds($Seconds)
while ((Get-Date) -lt $deadline) {
    $ts = (Get-Date).ToString('o')
    try {
        Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
            Where-Object { $_.InstanceId -match 'VID_239A|239A' } |
            ForEach-Object {
                "${ts} $($_.Status) | $($_.Class) | $($_.FriendlyName) | $($_.InstanceId)"
            } | Out-File -FilePath $LogPath -Append -Encoding utf8
    } catch {
        "${ts} ERROR $($_.Exception.Message)" | Out-File -FilePath $LogPath -Append -Encoding utf8
    }
    Start-Sleep -Milliseconds 700
}

"===== usb_vid_watch end $(Get-Date -Format o) =====" | Out-File -FilePath $LogPath -Append -Encoding utf8
Write-Host "Done. Log: $LogPath" -ForegroundColor Green
