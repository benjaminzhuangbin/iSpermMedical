@echo off
REM Nexus ADB Watchdog 2.2 - start.bat
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
set "REMOTE_DIR=/data/local/watchdog"

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH.
    exit /b 1
)

echo ============================================================
echo  Nexus ADB Watchdog 2.2 - start
echo ============================================================
adb devices

echo.
echo Stopping previous instance if any...
adb shell "if [ -f %REMOTE_DIR%/watchdog.pid ]; then kill `cat %REMOTE_DIR%/watchdog.pid` 2>/dev/null; fi"
adb shell "sleep 1"

echo Starting...
adb shell "nohup %REMOTE_DIR%/watchdog >/dev/null 2>&1 &"
adb shell "sleep 2"

echo.
echo PID:
adb shell "cat %REMOTE_DIR%/watchdog.pid 2>/dev/null"
echo.
echo Status:
adb shell "cat %REMOTE_DIR%/watchdog.status 2>/dev/null"
echo.
echo [OK] started
exit /b 0
