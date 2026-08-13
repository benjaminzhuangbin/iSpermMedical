@echo off
REM Start watchdog 2.1 on Android
setlocal EnableExtensions
cd /d "%~dp0"
set "REMOTE=/data/local/watchdog"

echo ============================================================
echo  Nexus ADB Watchdog 2.1 - start
echo ============================================================

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not in PATH
    pause
    exit /b 1
)

adb devices
echo.

echo Stopping previous instance if any...
adb shell "if [ -f %REMOTE%/watchdog.pid ]; then kill `cat %REMOTE%/watchdog.pid` 2>/dev/null; fi"
adb shell "sleep 1"

echo Starting...
adb shell "nohup %REMOTE%/watchdog >/dev/null 2>&1 &"
adb shell "sleep 2"

echo.
echo PID:
adb shell "cat %REMOTE%/watchdog.pid 2>/dev/null"
echo.
echo Version:
adb shell "%REMOTE%/watchdog -h"
echo.
echo Status:
adb shell "cat %REMOTE%/watchdog.status 2>/dev/null"
echo.
echo [OK] started
pause
exit /b 0
