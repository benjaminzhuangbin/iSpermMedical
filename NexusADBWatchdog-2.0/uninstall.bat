@echo off
REM ============================================================================
REM Nexus ADB Watchdog 2.0 - uninstall.bat
REM Stop watchdog and remove installed files. Does NOT touch adbd after stop.
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
echo  Nexus ADB Watchdog 2.0 - uninstall
echo ============================================================
adb devices

echo.
echo [1/3] Stopping watchdog via PID file...
adb shell "if [ -f %REMOTE_DIR%/watchdog.pid ]; then kill `cat %REMOTE_DIR%/watchdog.pid` 2>/dev/null; sleep 1; fi"

echo [2/3] Removing installed files...
adb shell "rm -f %REMOTE_DIR%/watchdog %REMOTE_DIR%/watchdog.conf %REMOTE_DIR%/watchdog.pid %REMOTE_DIR%/watchdog.status %REMOTE_DIR%/watchdog.log %REMOTE_DIR%/watchdog.log.1 %REMOTE_DIR%/watchdog.error %REMOTE_DIR%/watchdog.status.tmp"

echo [3/3] Removing directory if empty...
adb shell "rmdir %REMOTE_DIR% 2>/dev/null"

echo.
adb shell "ls -l %REMOTE_DIR% 2>/dev/null || echo (directory removed)"
echo.
echo [OK] Uninstall complete.
exit /b 0
