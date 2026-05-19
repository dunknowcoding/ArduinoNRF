#!/usr/bin/env python3
"""
Arduino nRF52840 Hardware Validation Script
Comprehensive board testing for ProMicro variant
"""

import subprocess
import sys
import time
import os
from pathlib import Path
from datetime import datetime
import serial.tools.list_ports
import threading
import re

# Fix Unicode encoding issues on Windows
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

class ValidationReport:
    def __init__(self):
        self.com3_present = False
        self.com3_vid_pid = None
        self.build_time = 0
        self.build_success = False
        self.upload_start_time = None
        self.upload_end_time = None
        self.upload_success = False
        self.cdc_enum_time = None
        self.cdc_enum_port = None
        self.second_upload_success = False
        self.final_ports = []
        self.errors = []

# ============================================================================
# TASK 1: Check COM ports and device info
# ============================================================================
def task1_check_com_ports():
    """Check for COM3 and nRF board presence"""
    print("\n" + "="*70)
    print("TASK 1: COM PORT DETECTION")
    print("="*70)
    
    report = ValidationReport()
    
    try:
        ports = list(serial.tools.list_ports.comports())
        print(f"\nFound {len(ports)} COM port(s):\n")
        
        for port in ports:
            vid_pid_str = f"(VID:{port.vid:04X} PID:{port.pid:04X})" if port.vid else "(VID/PID unknown)"
            print(f"  {port.device}: {port.description} {vid_pid_str}")
            
            if port.device == "COM3":
                report.com3_present = True
                report.com3_vid_pid = (port.vid, port.pid)
        
        print("\n" + "-"*70)
        if report.com3_present:
            vid, pid = report.com3_vid_pid
            print(f"[OK] COM3 DETECTED with VID:PID {vid:04X}:{pid:04X}")
            if vid == 0x239A:
                print("  [OK] Correct VID detected (Adafruit 239A)")
            else:
                print(f"  [WARNING] Expected VID 239A, found {vid:04X}")
        else:
            print("[ERROR] COM3 NOT DETECTED - Board may not be in bootloader mode")
            report.errors.append("COM3 not present")
        
    except Exception as e:
        print(f"✗ Error checking COM ports: {e}")
        report.errors.append(f"COM port check failed: {e}")
    
    return report

# ============================================================================
# TASK 2: Build the sketch
# ============================================================================
def task2_build_sketch():
    """Build the UsbSerial sketch"""
    print("\n" + "="*70)
    print("TASK 2: BUILD SKETCH")
    print("="*70)
    
    report = ValidationReport()
    
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
        report.build_time = build_end - build_start
        
        print(f"\nBuild output:\n{result.stdout}")
        
        if result.returncode == 0:
            report.build_success = True
            binary_path = work_dir / "build" / "arduinonrf_nrf52_promicro_nrf52840" / "UsbSerial.ino.bin"
            
            print("-"*70)
            print(f"✓ BUILD SUCCESSFUL in {report.build_time:.1f} seconds")
            
            if binary_path.exists():
                size = binary_path.stat().st_size
                print(f"  Binary size: {size} bytes")
            else:
                print(f"  Warning: Binary file not found at expected location")
        else:
            report.errors.append(f"Build failed: {result.stderr}")
            print("-"*70)
            print(f"✗ BUILD FAILED (exit code: {result.returncode})")
            print(f"Error output:\n{result.stderr}")
        
    except subprocess.TimeoutExpired:
        report.errors.append("Build timeout (>120s)")
        print("✗ BUILD TIMEOUT (exceeded 120 seconds)")
    except Exception as e:
        report.errors.append(f"Build error: {e}")
        print(f"✗ BUILD ERROR: {e}")
    
    return report

# ============================================================================
# TASK 3: Upload to COM3
# ============================================================================
def task3_upload_sketch(com_port="COM3"):
    """Upload sketch to board in bootloader mode"""
    print("\n" + "="*70)
    print("TASK 3: UPLOAD SKETCH (TO BOOTLOADER)")
    print("="*70)
    
    report = ValidationReport()
    
    try:
        work_dir = Path("F:/Arduino/driver/ArduinoNRF")
        config_file = work_dir / "arduino-cli.local.yaml"
        sketch_path = work_dir / "examples" / "UsbSerial"
        
        print(f"\nTarget port: {com_port}")
        print(f"FQBN: arduinonrf:nrf52:promicro_nrf52840")
        print("\nUploading...")
        
        report.upload_start_time = time.time()
        
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
        
        report.upload_end_time = time.time()
        upload_duration = report.upload_end_time - report.upload_start_time
        
        print(f"\nUpload output:\n{result.stdout}")
        
        if result.returncode == 0:
            report.upload_success = True
            print("-"*70)
            print(f"✓ UPLOAD SUCCESSFUL in {upload_duration:.1f} seconds")
        else:
            report.errors.append(f"Upload failed: {result.stderr}")
            print("-"*70)
            print(f"✗ UPLOAD FAILED (exit code: {result.returncode})")
            print(f"Error output:\n{result.stderr}")
        
    except subprocess.TimeoutExpired:
        report.errors.append("Upload timeout (>120s)")
        print("✗ UPLOAD TIMEOUT (exceeded 120 seconds)")
    except Exception as e:
        report.errors.append(f"Upload error: {e}")
        print(f"✗ UPLOAD ERROR: {e}")
    
    return report

# ============================================================================
# TASK 4: Monitor CDC enumeration
# ============================================================================
def task4_monitor_cdc_enumeration(timeout=120):
    """Monitor for CDC port enumeration after upload"""
    print("\n" + "="*70)
    print("TASK 4: CDC ENUMERATION MONITORING")
    print("="*70)
    
    report = ValidationReport()
    
    # Get baseline ports
    initial_ports = set([p.device for p in serial.tools.list_ports.comports()])
    print(f"\nBaseline ports: {sorted(initial_ports)}")
    print(f"Monitoring for new COM ports for {timeout} seconds...\n")
    
    enum_start = time.time()
    new_port = None
    new_ports_list = []
    
    try:
        while time.time() - enum_start < timeout:
            current_ports = set([p.device for p in serial.tools.list_ports.comports()])
            new_found = current_ports - initial_ports
            
            if new_found and not new_port:
                new_port = list(new_found)[0]
                enum_time = time.time() - enum_start
                report.cdc_enum_time = enum_time
                report.cdc_enum_port = new_port
                
                # Get VID/PID for new port
                for p in serial.tools.list_ports.comports():
                    if p.device == new_port:
                        print(f"✓ NEW PORT DETECTED: {new_port}")
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
                print("⚠ Warning: COM3 disappeared from system")
            
            time.sleep(0.5)
        
        # Final port list
        report.final_ports = sorted([p.device for p in serial.tools.list_ports.comports()])
        
        print("-"*70)
        if new_port:
            print(f"✓ CDC ENUMERATION SUCCESSFUL")
            print(f"  New port: {new_port} appeared in {report.cdc_enum_time:.1f}s")
        else:
            print(f"✗ CDC ENUMERATION TIMEOUT (no new port after {timeout}s)")
            report.errors.append("CDC enumeration timeout")
        
        print(f"\nFinal port list: {report.final_ports}")
        
    except Exception as e:
        report.errors.append(f"Enumeration monitoring error: {e}")
        print(f"✗ ERROR: {e}")
    
    return report

# ============================================================================
# TASK 5: Verify board state and attempt second upload
# ============================================================================
def task5_verify_and_second_upload(app_port):
    """Verify board state and attempt second upload from app mode"""
    print("\n" + "="*70)
    print("TASK 5: BOARD STATE VERIFICATION & SECOND UPLOAD")
    print("="*70)
    
    report = ValidationReport()
    
    if not app_port:
        print("\n⚠ Skipping second upload: No CDC port available")
        report.errors.append("No CDC port for second upload")
        return report
    
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
        
        print(f"\nSecond upload output:\n{result.stdout}")
        
        if result.returncode == 0:
            report.second_upload_success = True
            print("-"*70)
            print(f"✓ SECOND UPLOAD SUCCESSFUL in {upload_duration:.1f}s")
            print("  Reset-to-bootloader path verified!")
        else:
            report.errors.append(f"Second upload failed: {result.stderr}")
            print("-"*70)
            print(f"⚠ SECOND UPLOAD FAILED (exit code: {result.returncode})")
            print(f"  This may be normal if CDC reset is not configured")
            print(f"Error output:\n{result.stderr}")
        
    except subprocess.TimeoutExpired:
        print("⚠ SECOND UPLOAD TIMEOUT")
        report.errors.append("Second upload timeout")
    except Exception as e:
        print(f"⚠ SECOND UPLOAD ERROR: {e}")
        report.errors.append(f"Second upload error: {e}")
    
    return report

# ============================================================================
# Print final report
# ============================================================================
def print_final_report(r1, r2, r3, r4, r5):
    """Print consolidated validation report"""
    print("\n" + "="*70)
    print("VALIDATION SUMMARY")
    print("="*70)
    
    print(f"\n{'Board Present':<30} {'YES' if r1.com3_present else 'NO':<20}")
    if r1.com3_present and r1.com3_vid_pid:
        vid, pid = r1.com3_vid_pid
        print(f"{'  - VID:PID':<30} {vid:04X}:{pid:04X}")
    
    print(f"\n{'Build Status':<30} {'SUCCESS' if r2.build_success else 'FAILED'}")
    if r2.build_success:
        print(f"{'  - Build Time':<30} {r2.build_time:.1f}s")
    
    print(f"\n{'Upload Status (COM3)':<30} {'SUCCESS' if r3.upload_success else 'FAILED'}")
    if r3.upload_success and r3.upload_end_time and r3.upload_start_time:
        upload_duration = r3.upload_end_time - r3.upload_start_time
        print(f"{'  - Upload Time':<30} {upload_duration:.1f}s")
    
    print(f"\n{'CDC Enumeration':<30} {'SUCCESS' if r4.cdc_enum_port else 'TIMEOUT'}")
    if r4.cdc_enum_port:
        print(f"{'  - Port':<30} {r4.cdc_enum_port}")
        print(f"{'  - Enumeration Time':<30} {r4.cdc_enum_time:.1f}s")
    
    print(f"\n{'Second Upload':<30} {'SUCCESS' if r5.second_upload_success else 'NOT ATTEMPTED'}")
    
    print(f"\n{'Final Board State':<30} {', '.join(r4.final_ports) if r4.final_ports else 'No ports detected'}")
    
    total_errors = len(r1.errors) + len(r2.errors) + len(r3.errors) + len(r4.errors) + len(r5.errors)
    print(f"\n{'Total Issues Found':<30} {total_errors}")
    
    if total_errors > 0:
        print("\n" + "-"*70)
        print("ISSUES:")
        for r in [r1, r2, r3, r4, r5]:
            for error in r.errors:
                print(f"  ✗ {error}")
    
    print("\n" + "="*70)

# ============================================================================
# Main execution
# ============================================================================
def main():
    print("\n")
    print("╔" + "="*68 + "╗")
    print("║" + " "*68 + "║")
    print("║" + "  ARDUINO nRF52840 HARDWARE VALIDATION SCRIPT".center(68) + "║")
    print("║" + "  ProMicro Variant".center(68) + "║")
    print("║" + " "*68 + "║")
    print("╚" + "="*68 + "╝")
    
    print(f"\nStart time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Work directory: F:\\Arduino\\driver\\ArduinoNRF")
    
    start_time = time.time()
    
    # Execute all tasks
    r1 = task1_check_com_ports()
    r2 = task2_build_sketch()
    r3 = task3_upload_sketch()
    
    # Only monitor if build and upload succeeded
    if r2.build_success and r3.upload_success:
        r4 = task4_monitor_cdc_enumeration(timeout=120)
        r5 = task5_verify_and_second_upload(r4.cdc_enum_port)
    else:
        r4 = ValidationReport()
        r5 = ValidationReport()
        if not r2.build_success:
            print("\n⊘ Skipping upload: Build failed")
        if not r3.upload_success:
            print("\n⊘ Skipping enumeration monitoring: Upload failed")
    
    # Print final report
    print_final_report(r1, r2, r3, r4, r5)
    
    elapsed = time.time() - start_time
    print(f"\nValidation completed in {elapsed:.1f} seconds")
    print(f"End time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")

if __name__ == "__main__":
    main()
