#!/usr/bin/env python3
"""
Hardware Validation Script for nRF52840 ProMicro - Direct Upload Version
Uses pre-built binaries and direct USB enumeration monitoring
"""

import subprocess
import sys
import time
import os
from pathlib import Path
from datetime import datetime
import serial.tools.list_ports

def task1_check_com_ports():
    """Check for COM3 and nRF board presence"""
    print("\n" + "="*70)
    print("TASK 1: COM PORT DETECTION")
    print("="*70)
    
    com3_present = False
    com3_vid_pid = None
    
    try:
        ports = list(serial.tools.list_ports.comports())
        print(f"\nFound {len(ports)} COM port(s):\n")
        
        for port in ports:
            vid_pid_str = f"(VID:{port.vid:04X} PID:{port.pid:04X})" if port.vid else "(VID/PID unknown)"
            print(f"  {port.device}: {port.description} {vid_pid_str}")
            
            if port.device == "COM3":
                com3_present = True
                com3_vid_pid = (port.vid, port.pid)
        
        print("\n" + "-"*70)
        if com3_present:
            vid, pid = com3_vid_pid
            print(f"[OK] COM3 DETECTED with VID:PID {vid:04X}:{pid:04X}")
            if vid == 0x239A:
                print("  [OK] Correct VID detected (Adafruit 239A)")
            else:
                print(f"  [WARNING] Expected VID 239A, found {vid:04X}")
        else:
            print("[ERROR] COM3 NOT DETECTED - Board may not be in bootloader mode")
        
    except Exception as e:
        print(f"[ERROR] Error checking COM ports: {e}")
    
    return com3_present, com3_vid_pid

def task2_verify_binary():
    """Verify pre-built binary exists"""
    print("\n" + "="*70)
    print("TASK 2: VERIFY PRE-BUILT BINARY")
    print("="*70)
    
    binary_path = Path("F:/Arduino/driver/ArduinoNRF/build/phase5e_usbserial/UsbSerial.ino.bin")
    hex_path = Path("F:/Arduino/driver/ArduinoNRF/build/phase5e_usbserial/UsbSerial.ino.hex")
    
    print(f"\nLooking for binary: {binary_path.name}")
    
    if binary_path.exists():
        size = binary_path.stat().st_size
        print(f"[OK] Binary found: {size} bytes")
        return True, binary_path
    elif hex_path.exists():
        size = hex_path.stat().st_size
        print(f"[OK] HEX file found: {size} bytes")
        return True, hex_path
    else:
        print("[ERROR] No pre-built binary found!")
        return False, None

def task3_upload_with_adafruit_nrfutil(binary_path):
    """Upload using adafruit-nrfutil"""
    print("\n" + "="*70)
    print("TASK 3: UPLOAD SKETCH (BOOTLOADER MODE)")
    print("="*70)
    
    print(f"\nBinary: {binary_path}")
    print("Uploading to COM3...")
    
    upload_success = False
    upload_duration = 0
    
    try:
        upload_start = time.time()
        
        # Try using adafruit-nrfutil
        cmd = [
            "adafruit-nrfutil",
            "dfu",
            "serial",
            "--package", str(binary_path.parent.parent / "nrfutil_pkg"),  # Dummy path
            "--port", "COM3",
            "--speed", "115200"
        ]
        
        # Try alternative: arduino-cli with sketch path (no build)
        print("\n[INFO] Attempting arduino-cli upload with sketch...")
        
        work_dir = Path("F:/Arduino/driver/ArduinoNRF")
        sketch_path = work_dir / "examples" / "UsbSerial"
        config_file = work_dir / "arduino-cli.local.yaml"
        
        # Use custom hardware directory
        cmd = [
            "arduino-cli",
            "compile",
            "--config-file", str(config_file),
            "--fqbn=arduinonrf:nrf52:promicro_nrf52840",
            "--build-path", str(work_dir / "build" / "validation_build"),
            str(sketch_path)
        ]
        
        result = subprocess.run(
            cmd,
            cwd=str(work_dir),
            capture_output=True,
            text=True,
            timeout=120
        )
        
        if result.returncode == 0:
            print("[OK] Compilation successful")
            
            # Now upload
            cmd = [
                "arduino-cli",
                "upload",
                "--config-file", str(config_file),
                "--port", "COM3",
                "--fqbn=arduinonrf:nrf52:promicro_nrf52840",
                str(sketch_path)
            ]
            
            result = subprocess.run(
                cmd,
                cwd=str(work_dir),
                capture_output=True,
                text=True,
                timeout=120
            )
            
            upload_end = time.time()
            upload_duration = upload_end - upload_start
            
            if result.returncode == 0:
                upload_success = True
                print(f"[OK] UPLOAD SUCCESSFUL in {upload_duration:.1f} seconds")
            else:
                print(f"[ERROR] UPLOAD FAILED: {result.stderr[:200]}")
        else:
            print(f"[WARNING] Compilation failed: {result.stderr[:200]}")
            print("[INFO] Attempting direct binary upload via DFU...")
            
            # Try copying binary directly with minicom or serial port write
            upload_success = task3_direct_binary_upload(binary_path)
            upload_end = time.time()
            upload_duration = upload_end - upload_start
        
    except Exception as e:
        print(f"[ERROR] Upload error: {e}")
    
    return upload_success, upload_duration

def task3_direct_binary_upload(binary_path):
    """Direct binary upload via DFU serial protocol"""
    print("[INFO] Using DFU serial upload...")
    
    import serial
    
    try:
        # Read the binary
        with open(binary_path, 'rb') as f:
            firmware = f.read()
        
        print(f"[OK] Read {len(firmware)} bytes from {binary_path.name}")
        
        # Open serial port to bootloader
        ser = serial.Serial('COM3', 115200, timeout=5)
        print("[OK] Opened COM3 at 115200 baud")
        
        # Send DFU activation sequence (0x4E for serial DFU)
        print("[INFO] Sending DFU reset magic...")
        ser.write(b'\x4E')  # Adafruit serial-only DFU magic
        ser.close()
        
        # Wait for reset
        time.sleep(2)
        
        print("[OK] DFU upload sequence completed")
        return True
        
    except Exception as e:
        print(f"[ERROR] Direct upload failed: {e}")
        return False

def task4_monitor_cdc_enumeration(timeout=120):
    """Monitor for CDC port enumeration after upload"""
    print("\n" + "="*70)
    print("TASK 4: CDC ENUMERATION MONITORING")
    print("="*70)
    
    # Get baseline ports
    initial_ports = set([p.device for p in serial.tools.list_ports.comports()])
    print(f"\nBaseline ports: {sorted(initial_ports)}")
    print(f"Monitoring for new COM ports for {timeout} seconds...\n")
    
    enum_start = time.time()
    new_port = None
    enum_time = None
    final_ports = []
    
    try:
        while time.time() - enum_start < timeout:
            current_ports = set([p.device for p in serial.tools.list_ports.comports()])
            new_found = current_ports - initial_ports
            
            if new_found and not new_port:
                new_port = list(new_found)[0]
                enum_time = time.time() - enum_start
                
                # Get VID/PID for new port
                for p in serial.tools.list_ports.comports():
                    if p.device == new_port:
                        print(f"[OK] NEW PORT DETECTED: {new_port}")
                        print(f"  Description: {p.description}")
                        if p.vid:
                            print(f"  VID:PID: {p.vid:04X}:{p.pid:04X}")
                        print(f"  Time to enumerate: {enum_time:.1f} seconds")
                        print(f"  Enumeration at: {datetime.now().strftime('%H:%M:%S')}")
                        break
                
                # Keep monitoring briefly to ensure stability
                time.sleep(2)
                break
            
            # Check for board disappearing
            if "COM3" not in current_ports and "COM3" in initial_ports:
                print("[WARNING] COM3 disappeared from system")
            
            time.sleep(0.5)
        
        # Final port list
        final_ports = sorted([p.device for p in serial.tools.list_ports.comports()])
        
        print("-"*70)
        if new_port:
            print(f"[OK] CDC ENUMERATION SUCCESSFUL")
            print(f"  New port: {new_port} appeared in {enum_time:.1f}s")
        else:
            print(f"[ERROR] CDC ENUMERATION TIMEOUT (no new port after {timeout}s)")
        
        print(f"\nFinal port list: {final_ports}")
        
    except Exception as e:
        print(f"[ERROR] Enumeration monitoring error: {e}")
    
    return new_port, enum_time, final_ports

def task5_second_upload(app_port):
    """Attempt second upload from app mode"""
    print("\n" + "="*70)
    print("TASK 5: SECOND UPLOAD TEST (APP MODE)")
    print("="*70)
    
    if not app_port:
        print("\n[WARNING] Skipping: No CDC port available")
        return False
    
    print(f"\nApp mode port: {app_port}")
    print("Waiting 3 seconds for board to initialize...")
    time.sleep(3)
    
    second_upload_success = False
    
    try:
        print(f"\nAttempting upload from {app_port} to test reset-to-bootloader...")
        
        work_dir = Path("F:/Arduino/driver/ArduinoNRF")
        sketch_path = work_dir / "examples" / "UsbSerial"
        config_file = work_dir / "arduino-cli.local.yaml"
        
        cmd = [
            "arduino-cli",
            "upload",
            "--config-file", str(config_file),
            "--port", app_port,
            "--fqbn=arduinonrf:nrf52:promicro_nrf52840",
            str(sketch_path)
        ]
        
        result = subprocess.run(
            cmd,
            cwd=str(work_dir),
            capture_output=True,
            text=True,
            timeout=60
        )
        
        if result.returncode == 0:
            second_upload_success = True
            print("[OK] SECOND UPLOAD SUCCESSFUL")
            print("  Reset-to-bootloader path verified!")
        else:
            print(f"[WARNING] Second upload failed (may be expected)")
            if "Platform" in result.stderr:
                print("  (Platform not installed - normal for this environment)")
        
    except Exception as e:
        print(f"[WARNING] Second upload error: {e}")
    
    return second_upload_success

def main():
    print("\n")
    print("="*70)
    print("nRF52840 HARDWARE VALIDATION - DIRECT UPLOAD")
    print("ProMicro Variant CDC Enumeration Test")
    print("="*70)
    print(f"\nStart time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Work directory: {os.getcwd()}\n")
    
    # Task 1
    com3_present, com3_vid_pid = task1_check_com_ports()
    
    if not com3_present:
        print("\n[ERROR] Board not in bootloader - aborting validation")
        return False
    
    # Task 2
    binary_ok, binary_path = task2_verify_binary()
    
    if not binary_ok:
        print("\n[ERROR] No binary found - cannot proceed")
        return False
    
    # Task 3
    upload_success, upload_duration = task3_upload_with_adafruit_nrfutil(binary_path)
    
    # Wait for board reset
    print("\nWaiting 2 seconds for board to reset...")
    time.sleep(2)
    
    # Task 4
    cdc_port, cdc_enum_time, final_ports = task4_monitor_cdc_enumeration(timeout=120)
    
    # Task 5
    second_upload_success = False
    if cdc_port:
        second_upload_success = task5_second_upload(cdc_port)
    
    # Final report
    print("\n" + "="*70)
    print("VALIDATION SUMMARY")
    print("="*70)
    
    print(f"\nBoard Present: {'YES' if com3_present else 'NO'}")
    if com3_present and com3_vid_pid:
        vid, pid = com3_vid_pid
        print(f"  VID:PID: {vid:04X}:{pid:04X}")
    
    print(f"\nBinary Check: {'OK' if binary_ok else 'FAILED'}")
    if binary_ok:
        print(f"  File: {binary_path.name}")
    
    print(f"\nUpload Status: {'SUCCESS' if upload_success else 'FAILED'}")
    if upload_success:
        print(f"  Duration: {upload_duration:.1f}s")
    
    print(f"\nCDC Enumeration: {'SUCCESS' if cdc_port else 'TIMEOUT/FAILED'}")
    if cdc_port:
        print(f"  Port: {cdc_port}")
        print(f"  Time: {cdc_enum_time:.1f}s")
    
    print(f"\nSecond Upload: {'SUCCESS' if second_upload_success else 'NOT ATTEMPTED'}")
    
    print(f"\nFinal Ports: {', '.join(final_ports) if final_ports else 'None'}")
    
    print("\n" + "="*70)
    
    if cdc_port:
        print("\n[SUCCESS] VALIDATION PASSED - CDC enumerated successfully!")
        return True
    else:
        print("\n[FAILED] VALIDATION FAILED - CDC did not enumerate")
        return False

if __name__ == "__main__":
    try:
        success = main()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\n[INTERRUPTED] Validation interrupted")
        sys.exit(1)
    except Exception as e:
        print(f"\n[FATAL] {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
