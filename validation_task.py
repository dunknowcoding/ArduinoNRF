#!/usr/bin/env python3
import serial.tools.list_ports
import subprocess
import time
import sys
import threading
from datetime import datetime

print("\n" + "="*60)
print("ARDUINO nRF52840 VALIDATION TASK")
print("="*60 + "\n")

# TASK 1: Check COM ports and device info
print("TASK 1: COM PORTS AND DEVICE INFO")
print("-"*60)

ports = list(serial.tools.list_ports.comports())
com3_found = False
com3_info = {}

print(f"\nAvailable COM Ports ({len(ports)} total):")
for port in ports:
    vid_pid_str = f"{port.vid:04X}:{port.pid:04X}" if port.vid and port.pid else "Unknown"
    print(f"  {port.device}: {port.description}")
    print(f"    VID:PID = {vid_pid_str}")
    
    if port.device == "COM3":
        com3_found = True
        com3_info = {
            'device': port.device,
            'description': port.description,
            'vid': port.vid,
            'pid': port.pid,
            'vid_pid': vid_pid_str
        }

print(f"\nCOM3 Present: {'YES' if com3_found else 'NO'}")
if com3_found:
    print(f"  Description: {com3_info['description']}")
    print(f"  VID:PID: {com3_info['vid_pid']}")

print("\n" + "="*60)
print("TASK 2: BUILD SKETCH")
print("-"*60)

import os
os.chdir("f:\\Arduino\\driver\\ArduinoNRF")

build_cmd = [
    "arduino-cli",
    "compile",
    f"--config-file=arduino-cli.local.yaml",
    f"--fqbn=arduinonrf:nrf52:promicro_nrf52840",
    "examples/UsbSerial"
]

print(f"\nBuilding sketch: examples/UsbSerial")
print(f"FQBN: arduinonrf:nrf52:promicro_nrf52840")
print(f"Config: arduino-cli.local.yaml\n")

start_time = time.time()
try:
    result = subprocess.run(build_cmd, capture_output=True, text=True, timeout=300)
    build_time = time.time() - start_time
    
    print(f"Build completed in {build_time:.2f} seconds")
    
    if result.returncode == 0:
        print("Build Status: SUCCESS ✓")
    else:
        print("Build Status: FAILED ✗")
        print("\nBuild Output:")
        print(result.stdout)
        print("\nBuild Errors:")
        print(result.stderr)
except subprocess.TimeoutExpired:
    build_time = time.time() - start_time
    print(f"Build TIMEOUT after {build_time:.2f} seconds")
except Exception as e:
    print(f"Build Error: {e}")
    build_time = None

print("\n" + "="*60)
print("TASK 3: UPLOAD TO COM3 (BOOTLOADER MODE)")
print("-"*60)

if not com3_found:
    print("\nWARNING: COM3 not found. Attempting upload anyway...")

# Find the built binary
import glob
bin_files = glob.glob("f:\\Arduino\\driver\\ArduinoNRF\\build\\arduinonrf_nrf52_promicro_nrf52840\\*.bin")
if not bin_files:
    print("\nERROR: No built binary found in build directory")
    sys.exit(1)

binary_path = bin_files[0]
print(f"\nBinary: {binary_path}")
print(f"Target Port: COM3")
print(f"Mode: Bootloader (expecting VID 239A)\n")

upload_cmd = [
    "arduino-cli",
    "upload",
    f"--config-file=arduino-cli.local.yaml",
    f"--fqbn=arduinonrf:nrf52:promicro_nrf52840",
    f"--port=COM3",
    "examples/UsbSerial"
]

print("Starting upload...")
start_time = time.time()
try:
    result = subprocess.run(upload_cmd, capture_output=True, text=True, timeout=60)
    upload_time = time.time() - start_time
    
    print(f"Upload completed in {upload_time:.2f} seconds")
    print(f"Upload Status: {'SUCCESS ✓' if result.returncode == 0 else 'FAILED ✗'}")
    
    if result.returncode != 0:
        print("\nUpload Output:")
        print(result.stdout)
        print("\nUpload Errors:")
        print(result.stderr)
    else:
        print("\nUpload Output:")
        print(result.stdout[-500:])  # Last 500 chars
        
except subprocess.TimeoutExpired:
    upload_time = time.time() - start_time
    print(f"Upload TIMEOUT after {upload_time:.2f} seconds")
except Exception as e:
    print(f"Upload Error: {e}")
    upload_time = None

print("\n" + "="*60)
print("TASK 4: MONITOR PORT ENUMERATION (120 seconds)")
print("-"*60)

if upload_time and result.returncode == 0:
    print("\nMonitoring for CDC enumeration...")
    print("Looking for new COM ports (starting check at upload+1s)\n")
    
    # Get current ports before monitoring
    ports_before = set([p.device for p in serial.tools.list_ports.comports()])
    print(f"Initial ports: {sorted(ports_before)}")
    
    start_monitor = time.time()
    cdc_port = None
    enumeration_time = None
    
    while time.time() - start_monitor < 120:
        ports_now = set([p.device for p in serial.tools.list_ports.comports()])
        new_ports = ports_now - ports_before
        
        if new_ports:
            enumeration_time = time.time() - start_monitor
            cdc_port = sorted(list(new_ports))[0]
            print(f"\n✓ CDC ENUMERATED at {enumeration_time:.2f} seconds")
            print(f"  New Port: {cdc_port}")
            
            # Get info on new port
            for p in serial.tools.list_ports.comports():
                if p.device == cdc_port:
                    vid_pid = f"{p.vid:04X}:{p.pid:04X}" if p.vid and p.pid else "Unknown"
                    print(f"  Description: {p.description}")
                    print(f"  VID:PID: {vid_pid}")
            break
        
        time.sleep(0.5)
    
    if not cdc_port:
        print(f"\n✗ TIMEOUT: No new port detected after 120 seconds")
        print(f"Current ports: {sorted(ports_now)}")
else:
    print("\nSkipping enumeration monitor (upload did not complete)")
    enumeration_time = None
    cdc_port = None

print("\n" + "="*60)
print("VALIDATION SUMMARY")
print("-"*60)
print(f"\nCOM3 Present: {'YES' if com3_found else 'NO'}")
if com3_found:
    print(f"  VID:PID: {com3_info['vid_pid']}")
print(f"\nBuild Time: {build_time:.2f} seconds" if build_time else "\nBuild Time: FAILED")
print(f"Upload Time: {upload_time:.2f} seconds" if upload_time else "Upload Time: FAILED")
print(f"CDC Enumeration: {cdc_port} at {enumeration_time:.2f}s" if enumeration_time else "CDC Enumeration: TIMEOUT/FAILED")

# Final port check
print(f"\nFinal COM Ports:")
for p in serial.tools.list_ports.comports():
    vid_pid = f"{p.vid:04X}:{p.pid:04X}" if p.vid and p.pid else "Unknown"
    print(f"  {p.device}: {p.description} (VID:PID={vid_pid})")

print("\n" + "="*60 + "\n")
