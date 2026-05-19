#!/usr/bin/env python3
"""
Arduino nRF52840 ProMicro Validation Script
Tests COM port detection, build, upload, and enumeration
"""
import serial.tools.list_ports
import subprocess
import time
import os
import sys
from pathlib import Path

# Change to working directory
os.chdir(r"f:\Arduino\driver\ArduinoNRF")

print("\n" + "="*70)
print(" "*15 + "ARDUINO nRF52840 VALIDATION SCRIPT")
print("="*70)

# ============================================================================
# TASK 1: Check COM ports and device info
# ============================================================================
print("\nTASK 1: COM PORTS AND DEVICE INFO")
print("-"*70)

ports = list(serial.tools.list_ports.comports())
com3_found = False
com3_info = {}
nrf_devices = {}

print(f"\nAvailable COM Ports ({len(ports)} total):")
if ports:
    for i, port in enumerate(ports, 1):
        vid_pid_str = f"{port.vid:04X}:{port.pid:04X}" if port.vid and port.pid else "Unknown"
        print(f"  {i}. {port.device}: {port.description}")
        print(f"     VID:PID = {vid_pid_str}")
        
        # Check for nRF devices
        if "239a" in vid_pid_str.lower() or "arduino" in port.description.lower():
            nrf_devices[port.device] = {'desc': port.description, 'vidpid': vid_pid_str}
        
        if port.device == "COM3":
            com3_found = True
            com3_info = {
                'device': port.device,
                'description': port.description,
                'vid': port.vid,
                'pid': port.pid,
                'vid_pid': vid_pid_str
            }
else:
    print("  No COM ports found")

print(f"\n✓ COM3 Present: {'YES' if com3_found else 'NO'}")
if com3_found:
    print(f"  Device: {com3_info['device']}")
    print(f"  Description: {com3_info['description']}")
    print(f"  VID:PID: {com3_info['vid_pid']}")

if nrf_devices:
    print(f"\nnRF/Arduino Devices Found:")
    for dev, info in nrf_devices.items():
        print(f"  {dev}: {info['vidpid']} - {info['desc']}")

# ============================================================================
# TASK 2: Build the sketch
# ============================================================================
print("\n\nTASK 2: BUILD SKETCH")
print("-"*70)

build_cmd = [
    "arduino-cli",
    "compile",
    "--config-file=arduino-cli.local.yaml",
    "--fqbn=arduinonrf:nrf52:promicro_nrf52840",
    "examples/UsbSerial"
]

print(f"\nSketch: examples/UsbSerial")
print(f"FQBN: arduinonrf:nrf52:promicro_nrf52840")
print(f"Config: arduino-cli.local.yaml")
print(f"\nCommand: {' '.join(build_cmd)}\n")

build_success = False
build_time = 0

try:
    start_time = time.time()
    result = subprocess.run(build_cmd, capture_output=True, text=True, timeout=300)
    build_time = time.time() - start_time
    build_success = result.returncode == 0
    
    if build_success:
        print(f"✓ Build Status: SUCCESS")
        print(f"  Build Time: {build_time:.2f} seconds")
        
        # Check for build artifacts
        bin_path = Path("build/arduinonrf_nrf52_promicro_nrf52840/UsbSerial.ino.bin")
        if bin_path.exists():
            print(f"  Binary: {bin_path.resolve()}")
            print(f"  Size: {bin_path.stat().st_size} bytes")
    else:
        print(f"✗ Build Status: FAILED")
        print(f"  Build Time: {build_time:.2f} seconds")
        print(f"\nBuild Output:\n{result.stdout}")
        if result.stderr:
            print(f"\nBuild Errors:\n{result.stderr}")
            
except subprocess.TimeoutExpired:
    build_time = time.time() - start_time
    print(f"✗ Build TIMEOUT after {build_time:.2f} seconds")
except Exception as e:
    print(f"✗ Build Error: {e}")

# ============================================================================
# TASK 3: Upload to COM3 (Bootloader mode)
# ============================================================================
print("\n\nTASK 3: UPLOAD TO COM3 (BOOTLOADER MODE)")
print("-"*70)

if not build_success:
    print("\n✗ Build failed - skipping upload")
    upload_success = False
    upload_time = 0
else:
    if not com3_found:
        print("\n! Warning: COM3 not found. Attempting upload anyway...")
    
    upload_cmd = [
        "arduino-cli",
        "upload",
        "--config-file=arduino-cli.local.yaml",
        "--fqbn=arduinonrf:nrf52:promicro_nrf52840",
        "--port=COM3",
        "examples/UsbSerial"
    ]
    
    print(f"\nTarget Port: COM3")
    print(f"Mode: Bootloader (expecting VID 239A)")
    print(f"Command: {' '.join(upload_cmd)}\n")
    
    upload_success = False
    try:
        start_time = time.time()
        result = subprocess.run(upload_cmd, capture_output=True, text=True, timeout=60)
        upload_time = time.time() - start_time
        upload_success = result.returncode == 0
        
        if upload_success:
            print(f"✓ Upload Status: SUCCESS")
            print(f"  Upload Time: {upload_time:.2f} seconds")
            # Show last bit of output
            output_lines = result.stdout.strip().split('\n')
            if len(output_lines) > 5:
                print(f"  Output (last lines):")
                for line in output_lines[-5:]:
                    print(f"    {line}")
        else:
            print(f"✗ Upload Status: FAILED")
            print(f"  Upload Time: {upload_time:.2f} seconds")
            print(f"\nUpload Output:\n{result.stdout}")
            if result.stderr:
                print(f"\nUpload Errors:\n{result.stderr}")
                
    except subprocess.TimeoutExpired:
        upload_time = time.time() - start_time
        print(f"✗ Upload TIMEOUT after {upload_time:.2f} seconds")
        upload_success = False
    except Exception as e:
        print(f"✗ Upload Error: {e}")
        upload_success = False

# ============================================================================
# TASK 4: Monitor port enumeration
# ============================================================================
print("\n\nTASK 4: MONITOR PORT ENUMERATION (~120 seconds)")
print("-"*70)

if upload_success:
    print("\nMonitoring for CDC enumeration...")
    
    # Get current ports before monitoring
    ports_before = set([p.device for p in serial.tools.list_ports.comports()])
    print(f"Initial ports: {sorted(ports_before)}")
    print(f"Watching for new ports...\n")
    
    start_monitor = time.time()
    cdc_port = None
    enumeration_time = None
    monitor_duration = 120
    
    while time.time() - start_monitor < monitor_duration:
        ports_now = set([p.device for p in serial.tools.list_ports.comports()])
        new_ports = ports_now - ports_before
        
        if new_ports:
            enumeration_time = time.time() - start_monitor
            cdc_port = sorted(list(new_ports))[0]
            print(f"\n✓ CDC ENUMERATED")
            print(f"  Time: {enumeration_time:.2f} seconds")
            print(f"  New Port: {cdc_port}")
            
            # Get info on new port
            for p in serial.tools.list_ports.comports():
                if p.device == cdc_port:
                    vid_pid = f"{p.vid:04X}:{p.pid:04X}" if p.vid and p.pid else "Unknown"
                    print(f"  Description: {p.description}")
                    print(f"  VID:PID: {vid_pid}")
            break
        
        elapsed = time.time() - start_monitor
        if int(elapsed) % 10 == 0 and elapsed > 0:
            sys.stdout.write(f"  {int(elapsed)}s...")
            sys.stdout.flush()
        
        time.sleep(0.5)
    
    if not cdc_port:
        elapsed = time.time() - start_monitor
        print(f"\n✗ TIMEOUT: No new port detected after {elapsed:.2f} seconds")
        print(f"Current ports: {sorted(ports_now)}")
else:
    print("\n⊘ Skipped (upload did not complete)")
    cdc_port = None
    enumeration_time = None

# ============================================================================
# TASK 5: Second upload (if first succeeded)
# ============================================================================
print("\n\nTASK 5: SECOND UPLOAD ATTEMPT (from APP mode)")
print("-"*70)

if enumeration_time and cdc_port:
    print(f"\nAttempting second upload from {cdc_port}...")
    print(f"Testing reset-to-bootloader functionality\n")
    
    upload_cmd_2 = [
        "arduino-cli",
        "upload",
        "--config-file=arduino-cli.local.yaml",
        "--fqbn=arduinonrf:nrf52:promicro_nrf52840",
        f"--port={cdc_port}",
        "examples/UsbSerial"
    ]
    
    try:
        start_time = time.time()
        result = subprocess.run(upload_cmd_2, capture_output=True, text=True, timeout=60)
        upload_time_2 = time.time() - start_time
        
        if result.returncode == 0:
            print(f"✓ Second Upload Status: SUCCESS")
            print(f"  Upload Time: {upload_time_2:.2f} seconds")
            second_upload_success = True
        else:
            print(f"✗ Second Upload Status: FAILED")
            print(f"  Upload Time: {upload_time_2:.2f} seconds")
            print(f"  Output: {result.stdout}")
            second_upload_success = False
            
    except subprocess.TimeoutExpired:
        upload_time_2 = time.time() - start_time
        print(f"✗ Second Upload TIMEOUT after {upload_time_2:.2f} seconds")
        second_upload_success = False
    except Exception as e:
        print(f"✗ Second Upload Error: {e}")
        second_upload_success = False
else:
    print("\n⊘ Skipped (CDC enumeration did not occur)")
    second_upload_success = False

# ============================================================================
# SUMMARY
# ============================================================================
print("\n\n" + "="*70)
print(" "*20 + "VALIDATION SUMMARY")
print("="*70)

print(f"\n1. COM3 Detection:")
print(f"   Status: {'✓ YES' if com3_found else '✗ NO'}")
if com3_found:
    print(f"   VID:PID: {com3_info['vid_pid']}")

print(f"\n2. Build Sketch:")
print(f"   Status: {'✓ SUCCESS' if build_success else '✗ FAILED'}")
print(f"   Time: {build_time:.2f} seconds")

print(f"\n3. First Upload (COM3):")
print(f"   Status: {'✓ SUCCESS' if upload_success else '✗ FAILED'}")
if upload_success:
    print(f"   Time: {upload_time:.2f} seconds")

print(f"\n4. CDC Enumeration:")
if enumeration_time:
    print(f"   Status: ✓ DETECTED")
    print(f"   Port: {cdc_port}")
    print(f"   Time: {enumeration_time:.2f} seconds")
else:
    print(f"   Status: ✗ TIMEOUT/FAILED")

print(f"\n5. Second Upload (from app):")
if enumeration_time:
    print(f"   Status: {'✓ SUCCESS' if second_upload_success else '✗ FAILED'}")
    if second_upload_success:
        print(f"   Time: {upload_time_2:.2f} seconds")
else:
    print(f"   Status: ⊘ NOT ATTEMPTED")

print(f"\n6. Final Board State:")
final_ports = list(serial.tools.list_ports.comports())
if final_ports:
    print(f"   COM Ports available: {len(final_ports)}")
    for p in final_ports:
        vid_pid = f"{p.vid:04X}:{p.pid:04X}" if p.vid and p.pid else "Unknown"
        print(f"     {p.device}: {p.description} ({vid_pid})")
else:
    print(f"   No COM ports available")

print("\n" + "="*70 + "\n")
