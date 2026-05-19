#!/usr/bin/env python3
"""
Arduino nRF52840 Hardware Validation Script - ASCII Version
Comprehensive board testing for ProMicro variant
"""

import subprocess
import sys
import time
import os
from pathlib import Path
from datetime import datetime
import serial.tools.list_ports

# ============================================================================
# TASK 1: Check COM ports and device info
# ============================================================================
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

# ============================================================================
# TASK 2: Build the sketch
# ============================================================================
def task2_build_sketch():
    """Build the UsbSerial sketch"""
    print("\n" + "="*70)
    print("TASK 2: BUILD SKETCH")
    print("="*70)
    
    build_success = False
    build_time = 0
    
    try:
        work_dir = Path("F:/Arduino/driver/ArduinoNRF")
        sketch_path = work_dir / "examples" / "UsbSerial"
        config_file = work_dir / "arduino-cli.local.yaml"
        
        print(f"\nSketch: {sketch_path}")
        print(f"Config: {config_file}")
        print(f"FQBN: arduinonrf:nrf52:promicro_nrf52840")
        print("\nBuilding...")
        
        build_start = time.time()
        
        cmd = [
            "arduino-cli",
            "compile",
            f"--config-file={config_file}",
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
        
        build_end = time.time()
        build_time = build_end - build_start
        
        if result.returncode == 0:
            build_success = True
            print("-"*70)
            print(f"[OK] BUILD SUCCESSFUL in {build_time:.1f} seconds")
        else:
            print("-"*70)
            print(f"[ERROR] BUILD FAILED (exit code: {result.returncode})")
            print(f"Error output:\n{result.stderr}")
        
    except subprocess.TimeoutExpired:
        print("[ERROR] BUILD TIMEOUT (exceeded 120 seconds)")
    except Exception as e:
        print(f"[ERROR] BUILD ERROR: {e}")
    
    return build_success, build_time

# ============================================================================
# TASK 3: Upload to COM3
# ============================================================================
def task3_upload_sketch(com_port="COM3"):
    """Upload sketch to board in bootloader mode"""
    print("\n" + "="*70)
    print("TASK 3: UPLOAD SKETCH (TO BOOTLOADER)")
    print("="*70)
    
    upload_success = False
    upload_duration = 0
    upload_start_time = None
    upload_end_time = None
    
    try:
        work_dir = Path("F:/Arduino/driver/ArduinoNRF")
        config_file = work_dir / "arduino-cli.local.yaml"
        sketch_path = work_dir / "examples" / "UsbSerial"
        
        print(f"\nTarget port: {com_port}")
        print(f"FQBN: arduinonrf:nrf52:promicro_nrf52840")
        print("\nUploading...")
        
        upload_start_time = time.time()
        
        cmd = [
            "arduino-cli",
            "upload",
            f"--config-file={config_file}",
            f"--port={com_port}",
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
        
        upload_end_time = time.time()
        upload_duration = upload_end_time - upload_start_time
        
        if result.returncode == 0:
            upload_success = True
            print("-"*70)
            print(f"[OK] UPLOAD SUCCESSFUL in {upload_duration:.1f} seconds")
        else:
            print("-"*70)
            print(f"[ERROR] UPLOAD FAILED (exit code: {result.returncode})")
            print(f"Error output:\n{result.stderr}")
        
    except subprocess.TimeoutExpired:
        print("[ERROR] UPLOAD TIMEOUT (exceeded 120 seconds)")
    except Exception as e:
        print(f"[ERROR] UPLOAD ERROR: {e}")
    
    return upload_success, upload_duration, upload_start_time, upload_end_time

# ============================================================================
# TASK 4: Monitor CDC enumeration
# ============================================================================
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

# ============================================================================
# TASK 5: Verify board state and attempt second upload
# ============================================================================
def task5_verify_and_second_upload(app_port):
    """Verify board state and attempt second upload from app mode"""
    print("\n" + "="*70)
    print("TASK 5: BOARD STATE VERIFICATION & SECOND UPLOAD")
    print("="*70)
    
    second_upload_success = False
    
    if not app_port:
        print("\n[WARNING] Skipping second upload: No CDC port available")
        return second_upload_success
    
    print(f"\nApp mode port: {app_port}")
    print("Waiting 3 seconds for board to fully initialize...")
    time.sleep(3)
    
    # Try to detect reset-to-bootloader capability
    try:
        print("\nAttempting second upload to verify reset-to-bootloader...")
        
        work_dir = Path("F:/Arduino/driver/ArduinoNRF")
        config_file = work_dir / "arduino-cli.local.yaml"
        sketch_path = work_dir / "examples" / "UsbSerial"
        
        upload_start = time.time()
        
        cmd = [
            "arduino-cli",
            "upload",
            f"--config-file={config_file}",
            f"--port={app_port}",
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
        
        upload_end = time.time()
        upload_duration = upload_end - upload_start
        
        if result.returncode == 0:
            second_upload_success = True
            print("-"*70)
            print(f"[OK] SECOND UPLOAD SUCCESSFUL in {upload_duration:.1f}s")
            print("  Reset-to-bootloader path verified!")
        else:
            print("-"*70)
            print(f"[WARNING] SECOND UPLOAD FAILED (exit code: {result.returncode})")
            print(f"  This may be normal if CDC reset is not configured")
            print(f"Error output:\n{result.stderr}")
        
    except subprocess.TimeoutExpired:
        print("[WARNING] SECOND UPLOAD TIMEOUT")
    except Exception as e:
        print(f"[WARNING] SECOND UPLOAD ERROR: {e}")
    
    return second_upload_success

# ============================================================================
# Print final report
# ============================================================================
def print_final_report(com3_present, com3_vid_pid, build_success, build_time,
                       upload_success, upload_duration, cdc_port, cdc_enum_time,
                       final_ports, second_upload_success):
    """Print consolidated validation report"""
    print("\n" + "="*70)
    print("VALIDATION SUMMARY")
    print("="*70)
    
    print(f"\n{'Board Present':<30} {'YES' if com3_present else 'NO'}")
    if com3_present and com3_vid_pid:
        vid, pid = com3_vid_pid
        print(f"{'  - VID:PID':<30} {vid:04X}:{pid:04X}")
    
    print(f"\n{'Build Status':<30} {'SUCCESS' if build_success else 'FAILED'}")
    if build_success:
        print(f"{'  - Build Time':<30} {build_time:.1f}s")
    
    print(f"\n{'Upload Status (COM3)':<30} {'SUCCESS' if upload_success else 'FAILED'}")
    if upload_success:
        print(f"{'  - Upload Time':<30} {upload_duration:.1f}s")
    
    print(f"\n{'CDC Enumeration':<30} {'SUCCESS' if cdc_port else 'TIMEOUT'}")
    if cdc_port:
        print(f"{'  - Port':<30} {cdc_port}")
        print(f"{'  - Enumeration Time':<30} {cdc_enum_time:.1f}s")
    
    print(f"\n{'Second Upload':<30} {'SUCCESS' if second_upload_success else 'NOT ATTEMPTED/FAILED'}")
    
    print(f"\n{'Final Board State':<30} {', '.join(final_ports) if final_ports else 'No ports detected'}")
    
    print("\n" + "="*70)

# ============================================================================
# Main execution
# ============================================================================
def main():
    print("\n")
    print("="*70)
    print("ARDUINO nRF52840 HARDWARE VALIDATION SCRIPT")
    print("ProMicro Variant - CDC Enumeration Test")
    print("="*70)
    print(f"\nStart time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Work directory: {os.getcwd()}\n")
    
    # Task 1: Check COM ports
    com3_present, com3_vid_pid = task1_check_com_ports()
    
    if not com3_present:
        print("\n[ERROR] VALIDATION ABORTED - Board not found in bootloader mode")
        print("Please ensure the board is in bootloader mode (COM3 visible)")
        return False
    
    # Task 2: Build sketch
    build_success, build_time = task2_build_sketch()
    
    if not build_success:
        print("\n[ERROR] VALIDATION ABORTED - Build failed")
        return False
    
    # Task 3: Upload to bootloader
    upload_success, upload_duration, _, _ = task3_upload_sketch("COM3")
    
    if not upload_success:
        print("\n[ERROR] VALIDATION ABORTED - Upload to bootloader failed")
        return False
    
    # Wait a bit for board to reset
    print("\nWaiting 2 seconds for board to reset...")
    time.sleep(2)
    
    # Task 4: Monitor CDC enumeration
    cdc_port, cdc_enum_time, final_ports = task4_monitor_cdc_enumeration(timeout=120)
    
    # Task 5: Attempt second upload if CDC port enumerated
    second_upload_success = False
    if cdc_port:
        second_upload_success = task5_verify_and_second_upload(cdc_port)
    
    # Print final report
    print_final_report(com3_present, com3_vid_pid, build_success, build_time,
                      upload_success, upload_duration, cdc_port, cdc_enum_time,
                      final_ports, second_upload_success)
    
    # Overall result
    if cdc_port and cdc_enum_time < 120:
        print("\n[SUCCESS] VALIDATION PASSED - CDC enumeration successful!")
        return True
    else:
        print("\n[FAILED] VALIDATION FAILED - CDC enumeration timeout or not detected")
        return False

if __name__ == "__main__":
    try:
        success = main()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\n[INTERRUPTED] Validation interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n[FATAL] Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
