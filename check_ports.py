#!/usr/bin/env python3
"""Check COM ports and device info"""

import serial.tools.list_ports
import sys
import time

print('=== TASK 1: COM Port Enumeration ===')
print()

ports = serial.tools.list_ports.comports()
if not ports:
    print('No COM ports found!')
    sys.exit(1)

com3_found = False
com3_vid = None
com3_pid = None

print(f'Total ports found: {len(ports)}')
print()

for port in ports:
    print(f'Port: {port.device}')
    print(f'  Description: {port.description}')
    print(f'  Manufacturer: {port.manufacturer}')
    print(f'  Serial Number: {port.serial_number}')
    if port.vid:
        print(f'  VID: {port.vid:04x}')
    else:
        print(f'  VID: None')
    if port.pid:
        print(f'  PID: {port.pid:04x}')
    else:
        print(f'  PID: None')
    print()
    if port.device == 'COM3':
        com3_found = True
        com3_vid = port.vid
        com3_pid = port.pid

if com3_found:
    print('Status: COM3 is PRESENT')
    if com3_vid:
        print(f'COM3 VID/PID: {com3_vid:04x}:{com3_pid:04x}')
else:
    print('Status: COM3 NOT PRESENT')
