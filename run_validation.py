#!/usr/bin/env python3
"""
Minimal validation runner - execute validate_nrf.py directly
"""
import subprocess
import sys
import os

os.chdir(r"F:\Arduino\driver\ArduinoNRF")

# Run the validation script
try:
    result = subprocess.run(
        [sys.executable, "validate_nrf.py"],
        capture_output=False,
        text=True,
        timeout=300  # 5 minutes total timeout
    )
    sys.exit(result.returncode)
except subprocess.TimeoutExpired:
    print("\n✗ Validation script timed out after 5 minutes")
    sys.exit(1)
except Exception as e:
    print(f"\n✗ Error running validation: {e}")
    sys.exit(1)
