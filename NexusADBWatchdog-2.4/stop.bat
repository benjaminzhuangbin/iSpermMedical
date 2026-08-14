@echo off
REM Nexus ADB Watchdog 2.4 - stop.bat
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
set "REMOTE_DIR=/data/local/watchdog"

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH.
    exit /b 1
)

echo ============================================================
echo  Nexus ADB Watchdog 2.4 - stop
echo ============================================================
adb shell "if [ -f %REMOTE_DIR%/watchdog.pid ]; then kill `cat %REMOTE_DIR%/watchdog.pid` 2>/dev/null; sleep 1; fi"
adb shell "rm -f %REMOTE_DIR%/watchdog.pid"
echo [OK] stopped
exit /b 0
