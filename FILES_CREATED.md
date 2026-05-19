# Validation Files - Quick Reference

## Files Created for Arduino nRF52840 Validation

### Executable Scripts

#### 1. **validation.bat** ⭐ PRIMARY SCRIPT
- **Location**: `f:\Arduino\driver\ArduinoNRF\validation.bat`
- **Type**: Windows Batch Script
- **How to run**:
  ```
  Option A: Double-click in File Explorer
  Option B: cmd.exe → cd f:\Arduino\driver\ArduinoNRF → validation.bat
  ```
- **Duration**: 5-10 minutes
- **Tasks**: COM detection → Build → Upload → Enumeration monitoring
- **Output**: Real-time progress, final summary
- **Requires**: Python, arduino-cli, pyserial

#### 2. **validation_script.py** ALTERNATIVE PYTHON SCRIPT
- **Location**: `f:\Arduino\driver\ArduinoNRF\validation_script.py`
- **Type**: Python Script
- **How to run**:
  ```
  python f:\Arduino\driver\ArduinoNRF\validation_script.py
  ```
- **Duration**: 5-10 minutes (includes 120s enumeration wait)
- **Output**: Detailed status, timing for each task, VID/PID info
- **Best for**: Detailed analysis and logging

### Documentation Files

#### 3. **README_VALIDATION.md** ⭐ START HERE
- **Location**: `f:\Arduino\driver\ArduinoNRF\README_VALIDATION.md`
- **Type**: Executive Summary & Quick Start
- **Contents**:
  - 30-second overview of validation
  - Quick start instructions
  - Pre-validation checklist
  - Expected output examples
  - Success/failure criteria
- **Read time**: 5-10 minutes
- **Best for**: Understanding what validation will do

#### 4. **VALIDATION_QUICK_START.md** DETAILED GUIDE
- **Location**: `f:\Arduino\driver\ArduinoNRF\VALIDATION_QUICK_START.md`
- **Type**: Step-by-step how-to guide
- **Contents**:
  - Detailed prerequisites
  - Task-by-task instructions
  - Expected output for each task
  - Timing information
  - Troubleshooting guide
  - Support section
- **Read time**: 10-15 minutes
- **Best for**: Running validation and understanding results

#### 5. **VALIDATION_GUIDE.md** TECHNICAL REFERENCE
- **Location**: `f:\Arduino\driver\ArduinoNRF\VALIDATION_GUIDE.md`
- **Type**: Technical documentation
- **Contents**:
  - Board configuration details
  - Expected behavior explanation
  - Complete procedure for each task
  - DFU protocol details
  - Known issues & troubleshooting
- **Read time**: 15-20 minutes
- **Best for**: Technical understanding of the board

---

## Validation Workflow

```
START HERE:
  ↓
1. Read: README_VALIDATION.md (5 min)
   ↓
2. Check: Pre-validation checklist (2 min)
   ├─ Board connected
   ├─ Python installed
   ├─ arduino-cli installed
   └─ COM3 visible
   ↓
3. Run: validation.bat (5-10 min)
   ├─ Task 1: COM detection
   ├─ Task 2: Build sketch
   ├─ Task 3: Upload to bootloader
   ├─ Task 4: Monitor enumeration
   └─ Task 5: Report status
   ↓
4. Read: Output summary
   ├─ If SUCCESS → Board validated ✓
   └─ If FAILURE → Consult VALIDATION_QUICK_START.md
   ↓
5. Verify: Open serial monitor, watch for "tick" messages
   ↓
COMPLETE: Board is validated and running firmware
```

---

## How to Use These Files

### For First-Time Validation
1. **Read**: README_VALIDATION.md (5 min overview)
2. **Check**: Pre-validation checklist
3. **Run**: validation.bat
4. **Watch**: Real-time output
5. **Read**: Summary at end

### For Troubleshooting
1. **Check**: README_VALIDATION.md success/failure criteria
2. **Read**: VALIDATION_QUICK_START.md troubleshooting section
3. **Consult**: VALIDATION_GUIDE.md technical details
4. **Run**: Python script for more detailed logging

### For Understanding the Board
1. **Read**: README_VALIDATION.md board configuration
2. **Read**: VALIDATION_GUIDE.md technical specs
3. **Check**: hardware/arduinonrf/nrf52/boards.txt for exact config
4. **Review**: examples/UsbSerial/UsbSerial.ino for firmware

---

## Quick Reference Commands

### Check Prerequisites
```batch
REM Check Python
python --version

REM Check arduino-cli  
arduino-cli version

REM Check COM3 exists
mode COM3

REM List all COM ports
wmic path win32_serialport get name

REM Check pyserial
python -c "import serial.tools.list_ports; print('OK')"
```

### Run Validation
```batch
REM Method 1: Batch script
cd f:\Arduino\driver\ArduinoNRF
validation.bat

REM Method 2: Python script
python validation_script.py

REM Method 3: Manual tasks (see VALIDATION_GUIDE.md)
```

### Verify Results
```batch
REM After validation completes, check COM ports
wmic path win32_serialport get name

REM Expected output (example):
REM COM3  (bootloader, VID 239A)
REM COM4  (CDC app, VID 239A)
```

---

## Expected Results Summary

### ✓ Successful Validation Output
```
TASK 1: COM PORTS - SUCCESS
  COM3 found with VID:PID 239A:00B3

TASK 2: BUILD SKETCH - SUCCESS
  Binary created: 45KB

TASK 3: UPLOAD - SUCCESS
  Upload completed in 15 seconds

TASK 4: ENUMERATION - SUCCESS
  New COM4 enumerated in 3 seconds

TASK 5: STATUS - SUCCESS
  Board running, firmware active

OVERALL: VALIDATION PASSED ✓
```

### ✗ Failed Validation (Example)
```
TASK 1: COM PORTS - FAILED
  COM3 not found

ACTION NEEDED:
  - Check USB cable
  - Verify Device Manager shows device
  - See VALIDATION_QUICK_START.md troubleshooting
```

---

## File Dependencies

```
validation.bat
├── Requires: python.exe (in PATH)
├── Requires: arduino-cli.exe (in PATH)
├── Requires: arduino-cli.local.yaml (in current dir)
├── Requires: examples/UsbSerial/ (sketch to build)
├── Requires: hardware/arduinonrf/nrf52/ (platform files)
└── Produces: Build artifacts in build/

validation_script.py
├── Requires: python 3.7+
├── Requires: pyserial module
├── Requires: arduino-cli
├── Requires: Board connected to USB
└── Produces: Console output (no files written)

Documentation files (*.md)
├── README_VALIDATION.md
├── VALIDATION_QUICK_START.md
└── VALIDATION_GUIDE.md
└── No dependencies, read-only
```

---

## When to Use Each File

| Situation | Use This File |
|-----------|---------------|
| Don't know where to start | README_VALIDATION.md |
| Want step-by-step instructions | VALIDATION_QUICK_START.md |
| Need technical details | VALIDATION_GUIDE.md |
| Ready to validate | Run validation.bat |
| Want detailed output | Run validation_script.py |
| Validation failed | VALIDATION_QUICK_START.md (troubleshooting) |
| Want to understand board config | VALIDATION_GUIDE.md |
| Building other sketches | VALIDATION_GUIDE.md |

---

## File Sizes & Read Times

| File | Size | Read Time |
|------|------|-----------|
| validation.bat | ~5 KB | - |
| validation_script.py | ~12 KB | - |
| README_VALIDATION.md | ~10 KB | 5-10 min |
| VALIDATION_QUICK_START.md | ~9 KB | 10-15 min |
| VALIDATION_GUIDE.md | ~7 KB | 15-20 min |

---

## Support & Troubleshooting

### Common Issues

**Issue**: "arduino-cli: command not found"
**Solution**: See VALIDATION_QUICK_START.md prerequisites

**Issue**: "Python: command not found"
**Solution**: Install Python 3.7+ from python.org

**Issue**: "COM3 not found"
**Solution**: See troubleshooting in VALIDATION_QUICK_START.md

**Issue**: "Build failed"
**Solution**: Check platform files in hardware/arduinonrf/nrf52/

**Issue**: "Upload failed"
**Solution**: Ensure board in bootloader mode, check COM port number

**Issue**: "Enumeration timeout"
**Solution**: Check USB cable, try different port, see troubleshooting guide

---

## Next Steps

### Immediate (< 5 minutes)
1. ✓ Verify board connected
2. ✓ Run validation.bat
3. ✓ Read summary output

### Short-term (5-30 minutes)
1. ✓ Read README_VALIDATION.md
2. ✓ Understand validation flow
3. ✓ Fix any issues if needed
4. ✓ Verify firmware running

### Longer-term
1. Modify sketches as needed
2. Test board features
3. Deploy to production
4. Keep backup of validated firmware

---

## Archive & Reference

All validation files are stored in: `f:\Arduino\driver\ArduinoNRF\`

Keep these files for:
- Future validation runs
- Troubleshooting reference
- Documentation of setup
- Training other developers

---

**Status**: All validation files created and ready to use
**Next Action**: Read README_VALIDATION.md and run validation.bat
**Time to Validation**: 5-15 minutes (5 min read + 10 min validation)
