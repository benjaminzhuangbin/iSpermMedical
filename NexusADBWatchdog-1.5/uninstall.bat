@echo off
REM ============================================================================
REM Nexus ADB Watchdog 1.5 - uninstall.bat
REM Stop the watchdog daemon and remove installed files from the device.
REM NEVER touches adbd, system properties, or reboot.
REM ============================================================================
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

set "REMOTE_DIR=/data/local/watchdog"

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH.
    exit /b 1
)

echo ============================================================
echo  Nexus ADB Watchdog 1.5 - uninstall
echo ============================================================
echo  Remote dir : %REMOTE_DIR%
echo ============================================================

adb devices

echo.
echo [1/3] Stopping watchdog via PID file (does NOT touch adbd)...
adb shell "if [ -f %REMOTE_DIR%/watchdog.pid ]; then kill `cat %REMOTE_DIR%/watchdog.pid` 2>/dev/null; sleep 1; fi"

echo [2/3] Removing installed files...
adb shell "rm -f %REMOTE_DIR%/watchdog %REMOTE_DIR%/watchdog.conf %REMOTE_DIR%/watchdog.pid %REMOTE_DIR%/watchdog.status %REMOTE_DIR%/watchdog.log %REMOTE_DIR%/watchdog.log.1 %REMOTE_DIR%/watchdog.status.tmp"

echo [3/3] Removing directory if empty...
adb shell "rmdir %REMOTE_DIR% 2>/dev/null"

echo.
echo Remaining remote files (if any):
adb shell "ls -l %REMOTE_DIR% 2>/dev/null || echo (directory removed)"

echo.
echo [OK] Uninstall complete.
echo NOTE: adbd and TCP ADB settings were NOT modified.
exit /b 0
