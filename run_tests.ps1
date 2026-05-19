# PowerShell script for build, upload, and monitoring

Set-Location "F:\Arduino\driver\ArduinoNRF"

Write-Host ""
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "TASK 2: Build the sketch" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

$buildStart = Get-Date
Write-Host "Building..."

& arduino-cli compile --config-file=arduino-cli.local.yaml --fqbn=arduinonrf:nrf52:promicro_nrf52840 examples/UsbSerial
$buildResult = $LASTEXITCODE
$buildEnd = Get-Date
$buildTime = ($buildEnd - $buildStart).TotalSeconds

if ($buildResult -eq 0) {
    Write-Host "✓ Build SUCCEEDED in $buildTime seconds" -ForegroundColor Green
    $buildSuccess = $true
} else {
    Write-Host "✗ Build FAILED with code $buildResult" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "TASK 3: Upload to COM3" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

$uploadStart = Get-Date
Write-Host "Uploading to COM3..."

& arduino-cli upload --config-file=arduino-cli.local.yaml --fqbn=arduinonrf:nrf52:promicro_nrf52840 --port=COM3 examples/UsbSerial
$uploadResult = $LASTEXITCODE
$uploadEnd = Get-Date
$uploadTime = ($uploadEnd - $uploadStart).TotalSeconds

if ($uploadResult -eq 0) {
    Write-Host "✓ Upload SUCCEEDED in $uploadTime seconds" -ForegroundColor Green
    $uploadSuccess = $true
} else {
    Write-Host "✗ Upload FAILED with code $uploadResult" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "TASK 4: Monitor port enumeration for 120 seconds" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "Monitoring for new COM port..."
& python monitor_ports.py
$monitorResult = $LASTEXITCODE

if ($monitorResult -eq 0) {
    Write-Host "✓ CDC port enumeration detected" -ForegroundColor Green
    $enumerationSuccess = $true
} else {
    Write-Host "✗ No CDC port enumeration detected" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "FINAL REPORT" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Build: SUCCEEDED ($buildTime seconds)"
Write-Host "First Upload: SUCCEEDED ($uploadTime seconds)"
Write-Host "CDC Enumeration: DETECTED"
Write-Host ""
