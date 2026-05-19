#!/usr/bin/env python3
"""
Execute all tasks: COM port check, build, upload, monitor, and second upload
"""

import serial.tools.list_ports
import subprocess
import sys
import time
import os
from datetime import datetime

os.chdir('F:\\Arduino\\driver\\ArduinoNRF')

def get_com_ports():
    """Get list of COM ports with details"""
    ports = {}
    for port in serial.tools.list_ports.comports():
        ports[port.device] = {
            'description': port.description,
            'manufacturer': port.manufacturer,
            'serial': port.serial_number,
            'vid': port.vid,
            'pid': port.pid
        }
    return ports

def print_ports(ports, title="COM Ports"):
    """Print port information"""
    print(f'\n{title}:')
    if not ports:
        print('  No ports found')
        return
    for port, info in ports.items():
        print(f'  {port}: {info["description"]}')
        if info['vid']:
            print(f'    VID/PID: {info["vid"]:04x}:{info["pid"]:04x}')
        if info['manufacturer']:
            print(f'    Manufacturer: {info["manufacturer"]}')

# ============================================================================
# TASK 1: Check COM ports and device info
# ============================================================================
print('\n' + '='*70)
print('TASK 1: Check COM ports and device info')
print('='*70)

ports_before = get_com_ports()
print_ports(ports_before, "Initial COM Ports")

com3_present = 'COM3' in ports_before
print(f'\nCOM3 Present: {"YES" if com3_present else "NO"}')
if com3_present:
    com3_info = ports_before['COM3']
    if com3_info['vid']:
        print(f'COM3 VID/PID: {com3_info["vid"]:04x}:{com3_info["pid"]:04x}')

# ============================================================================
# TASK 2: Build the sketch
# ============================================================================
print('\n' + '='*70)
print('TASK 2: Build the sketch')
print('='*70)

build_cmd = [
    'arduino-cli', 'compile',
    '--config-file=arduino-cli.local.yaml',
    '--fqbn=arduinonrf:nrf52:promicro_nrf52840',
    'examples/UsbSerial'
]

print(f'\nBuild command: {" ".join(build_cmd)}')
print('Building...')

build_start = time.time()
try:
    result = subprocess.run(build_cmd, capture_output=True, text=True, timeout=300)
    build_time = time.time() - build_start
    
    if result.returncode == 0:
        print(f'✓ Build SUCCEEDED in {build_time:.2f} seconds')
        build_success = True
    else:
        print(f'✗ Build FAILED after {build_time:.2f} seconds')
        print('\nBuild output:')
        print(result.stdout)
        if result.stderr:
            print('Build stderr:')
            print(result.stderr)
        build_success = False
except subprocess.TimeoutExpired:
    print('✗ Build TIMEOUT (exceeded 5 minutes)')
    build_success = False
    build_time = 300
except Exception as e:
    print(f'✗ Build ERROR: {e}')
    build_success = False
    build_time = 0

# ============================================================================
# TASK 3: Upload to COM3
# ============================================================================
print('\n' + '='*70)
print('TASK 3: Upload to COM3')
print('='*70)

if not build_success:
    print('Skipping upload - build failed')
    upload_success = False
    upload_time = 0
else:
    if not com3_present:
        print('Skipping upload - COM3 not present')
        upload_success = False
        upload_time = 0
    else:
        upload_cmd = [
            'arduino-cli', 'upload',
            '--config-file=arduino-cli.local.yaml',
            '--fqbn=arduinonrf:nrf52:promicro_nrf52840',
            '--port=COM3',
            'examples/UsbSerial'
        ]
        
        print(f'\nUpload command: {" ".join(upload_cmd)}')
        print('Uploading...')
        
        upload_start = time.time()
        try:
            result = subprocess.run(upload_cmd, capture_output=True, text=True, timeout=120)
            upload_time = time.time() - upload_start
            
            if result.returncode == 0:
                print(f'✓ Upload SUCCEEDED in {upload_time:.2f} seconds')
                upload_success = True
            else:
                print(f'✗ Upload FAILED after {upload_time:.2f} seconds')
                print('\nUpload output:')
                print(result.stdout)
                if result.stderr:
                    print('Upload stderr:')
                    print(result.stderr)
                upload_success = False
        except subprocess.TimeoutExpired:
            print('✗ Upload TIMEOUT (exceeded 2 minutes)')
            upload_success = False
            upload_time = 120
        except Exception as e:
            print(f'✗ Upload ERROR: {e}')
            upload_success = False
            upload_time = 0

# ============================================================================
# TASK 4: Monitor port enumeration
# ============================================================================
print('\n' + '='*70)
print('TASK 4: Monitor port enumeration')
print('='*70)

enumeration_time = None
enumeration_port = None

if upload_success:
    print('\nMonitoring for CDC serial port enumeration (120 seconds)...')
    
    monitor_start = time.time()
    monitor_interval = 1  # Check every second
    max_wait = 120
    
    cdc_detected = False
    
    while time.time() - monitor_start < max_wait:
        current_ports = get_com_ports()
        new_ports = set(current_ports.keys()) - set(ports_before.keys())
        
        if new_ports:
            elapsed = time.time() - monitor_start
            print(f'\n✓ New port detected at {elapsed:.1f} seconds: {new_ports}')
            enumeration_port = list(new_ports)[0]
            enumeration_time = elapsed
            cdc_detected = True
            
            # Print details of new port
            new_port_info = current_ports[enumeration_port]
            print(f'  Description: {new_port_info["description"]}')
            if new_port_info['vid']:
                print(f'  VID/PID: {new_port_info["vid"]:04x}:{new_port_info["pid"]:04x}')
            
            break
        
        time.sleep(monitor_interval)
    
    if not cdc_detected:
        print(f'✗ No new port enumeration detected after {max_wait} seconds')
else:
    print('Skipping enumeration monitoring - upload failed')

# ============================================================================
# TASK 5: Second upload (if first succeeded and port enumerated)
# ============================================================================
print('\n' + '='*70)
print('TASK 5: Second upload (if conditions met)')
print('='*70)

second_upload_success = False
second_upload_time = 0

if upload_success and enumeration_port:
    second_upload_cmd = [
        'arduino-cli', 'upload',
        '--config-file=arduino-cli.local.yaml',
        '--fqbn=arduinonrf:nrf52:promicro_nrf52840',
        f'--port={enumeration_port}',
        'examples/UsbSerial'
    ]
    
    print(f'\nSecond upload to {enumeration_port}')
    print(f'Command: {" ".join(second_upload_cmd)}')
    print('Uploading...')
    
    second_start = time.time()
    try:
        result = subprocess.run(second_upload_cmd, capture_output=True, text=True, timeout=120)
        second_upload_time = time.time() - second_start
        
        if result.returncode == 0:
            print(f'✓ Second upload SUCCEEDED in {second_upload_time:.2f} seconds')
            second_upload_success = True
        else:
            print(f'✗ Second upload FAILED after {second_upload_time:.2f} seconds')
            print('\nUpload output:')
            print(result.stdout)
            if result.stderr:
                print('Upload stderr:')
                print(result.stderr)
    except subprocess.TimeoutExpired:
        print('✗ Second upload TIMEOUT')
    except Exception as e:
        print(f'✗ Second upload ERROR: {e}')
else:
    print('Conditions not met for second upload')

# ============================================================================
# FINAL REPORT
# ============================================================================
print('\n' + '='*70)
print('FINAL REPORT')
print('='*70)

print(f'\n1. COM3 Present: {"YES" if com3_present else "NO"}')
if com3_present and ports_before['COM3']['vid']:
    print(f'   VID/PID: {ports_before["COM3"]["vid"]:04x}:{ports_before["COM3"]["pid"]:04x}')

print(f'\n2. Build: {"SUCCESS" if build_success else "FAILED"} ({build_time:.2f}s)')

print(f'\n3. First Upload: {"SUCCESS" if upload_success else "FAILED"} ({upload_time:.2f}s)')

if enumeration_time is not None:
    print(f'\n4. CDC Enumeration: {enumeration_port} at {enumeration_time:.1f}s')
else:
    print(f'\n4. CDC Enumeration: NOT DETECTED')

print(f'\n5. Second Upload: {"SUCCESS" if second_upload_success else "SKIPPED/FAILED"} ({second_upload_time:.2f}s)')

ports_final = get_com_ports()
print_ports(ports_final, "Final COM Ports")

print('\n' + '='*70)
