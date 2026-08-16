@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "REMOTE=/data/local/watchdog"

echo ============================================================
echo  Nexus ADB Watchdog - which version is on Android?
echo ============================================================

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not in PATH
    pause
    exit /b 1
)

adb devices
echo.

echo [1] Binary help string:
adb shell "%REMOTE%/watchdog -h"
echo.

echo [2] Version strings inside binary:
adb shell "strings %REMOTE%/watchdog" 2>nul | findstr /i "Watchdog 2."
echo.

echo [3] Recent log start markers:
adb shell "cat %REMOTE%/watchdog.log" 2>nul | findstr /i "started"
echo.

echo [4] Current status:
adb shell "cat %REMOTE%/watchdog.status" 2>nul
echo.

echo [5] Running PID:
adb shell "cat %REMOTE%/watchdog.pid" 2>nul
echo.
pause
exit /b 0
