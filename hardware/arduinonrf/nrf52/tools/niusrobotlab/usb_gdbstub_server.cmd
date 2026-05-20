@echo off
REM Arduino IDE 2 / cortex-debug pseudo-server wrapper.
REM
REM cortex-debug (servertype="openocd") OWNS the gdb port: it finds a free
REM TCP port, then launches this "openocd" with openocd-style flags such as
REM   -c "gdb_port 50000" -c "tcl_port 50001" -c "telnet_port 50002"
REM and afterwards connects gdb to localhost:50000. The port is dynamic and
REM differs every session, so we MUST listen on whatever gdb_port cortex-debug
REM hands us -- hardcoding 3335 made cortex-debug connect to a dead port and
REM tear the session down (read ECONNRESET). arduino-cli debug (CLI) instead
REM passes `-c "gdb_port pipe"`; for that (and if no gdb_port is given) we
REM fall back to the default 3335.
REM
REM Once the TCP listener is up the bridge prints
REM   Listening on port <N> for gdb connections
REM which matches cortex-debug's default openocd serverReady regex.

setlocal enabledelayedexpansion
set "SCRIPT_DIR=%~dp0"
set "BRIDGE=%SCRIPT_DIR%usb_gdbstub_bridge.ps1"
set "BOARD=promicro_nrf52840"
set "TCP_PORT=3335"
set "BAUD=115200"
set "WANT_PORT="

REM Scan the openocd-style args for the gdb port. Handle both the combined
REM form (`-c "gdb_port 50000"` -> one arg "gdb_port 50000") and the split
REM form (`gdb_port` then `50000` as two args).
:parse
if "%~1"=="" goto resolve
set "ARG=%~1"
if /i "!ARG!"=="gdb_port" (
    set "WANT_PORT=NEXT"
    shift
    goto parse
)
if "!WANT_PORT!"=="NEXT" (
    set "WANT_PORT=!ARG!"
    shift
    goto parse
)
echo !ARG! | findstr /b /i /c:"gdb_port " >nul
if not errorlevel 1 (
    for /f "tokens=2" %%a in ("!ARG!") do set "WANT_PORT=%%a"
)
shift
goto parse

:resolve
REM Accept WANT_PORT only if it is purely numeric (ignore "pipe", blanks).
if defined WANT_PORT (
    echo !WANT_PORT!| findstr /r "^[0-9][0-9]*$" >nul
    if not errorlevel 1 set "TCP_PORT=!WANT_PORT!"
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%BRIDGE%" -Board "%BOARD%" -TcpPort !TCP_PORT! -BaudRate %BAUD% -PreferServiceCdc
endlocal
