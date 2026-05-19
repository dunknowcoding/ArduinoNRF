@echo off
REM Arduino IDE 2 / cortex-debug pseudo-server wrapper.
REM
REM cortex-debug (with servertype="openocd") spawns this script with
REM openocd-style flags (-d3 -s <searchDir> -f <cfg> -c "gdb_port 3335"
REM etc) AND arduino-cli debug (CLI) spawns it with gdb-pipe-mode flags
REM (--file "" -c "gdb_port pipe" -c "telnet_port 0"). We ignore ALL
REM positional arguments; the bridge auto-picks the SERVICE CDC for the
REM board declared in platform.txt and listens on the gdbPort that the
REM matching boards.txt buildprofile=usbgdbstub override pins to 3335.
REM
REM Right after the TCP listener is ready the bridge prints
REM   Listening on port 3335 for gdb connections
REM which matches cortex-debug's default openocd serverReady regex. That
REM is the contract that lets IDE 2 spawn this wrapper directly from a
REM Debug-button click without a separate "start bridge" terminal.

setlocal
set "SCRIPT_DIR=%~dp0"
set "BRIDGE=%SCRIPT_DIR%usb_gdbstub_bridge.ps1"
set "BOARD=promicro_nrf52840"
set "TCP_PORT=3335"
set "BAUD=115200"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%BRIDGE%" -Board "%BOARD%" -TcpPort %TCP_PORT% -BaudRate %BAUD% -PreferServiceCdc

endlocal
