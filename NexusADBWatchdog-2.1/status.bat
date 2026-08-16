@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "REMOTE=/data/local/watchdog"
echo ============================================================
echo  Nexus ADB Watchdog 2.1 - status
echo ============================================================
where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not in PATH
    pause
    exit /b 1
)
echo --- version ---
adb shell "%REMOTE%/watchdog -h"
echo.
echo --- watchdog.status ---
adb shell "cat %REMOTE%/watchdog.status"
echo.
echo --- watchdog.pid ---
adb shell "cat %REMOTE%/watchdog.pid 2>/dev/null"
echo.
echo --- watchdog.conf ---
adb shell "cat %REMOTE%/watchdog.conf"
echo.
pause
exit /b 0
