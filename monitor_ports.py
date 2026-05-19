#!/usr/bin/env python
"""Monitor for new COM port enumeration"""

import serial.tools.list_ports
import time
import sys

def get_com_ports():
    """Get list of COM ports"""
    ports = {}
    for port in serial.tools.list_ports.comports():
        ports[port.device] = {
            'description': port.description,
            'vid': port.vid,
            'pid': port.pid
        }
    return ports

# Get initial ports
initial_ports = get_com_ports()
print(f"Initial ports: {list(initial_ports.keys())}")
print()

# Monitor for 120 seconds
monitor_start = time.time()
max_wait = 120
monitor_interval = 1

found_new_port = False

while time.time() - monitor_start < max_wait:
    current_ports = get_com_ports()
    new_ports = set(current_ports.keys()) - set(initial_ports.keys())
    
    if new_ports:
        elapsed = time.time() - monitor_start
        print(f"✓ New port detected at {elapsed:.1f} seconds: {new_ports}")
        
        for port in new_ports:
            info = current_ports[port]
            print(f"  {port}: {info['description']}")
            if info['vid']:
                print(f"  VID/PID: {info['vid']:04x}:{info['pid']:04x}")
        
        found_new_port = True
        break
    
    time.sleep(monitor_interval)

if not found_new_port:
    print(f"✗ No new port enumeration detected after {max_wait} seconds")
    sys.exit(1)

print()
sys.exit(0)
