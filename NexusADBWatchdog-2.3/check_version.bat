@echo off
REM Nexus ADB Watchdog 2.3 - check_version.bat
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
set "REMOTE_DIR=/data/local/watchdog"

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH.
    exit /b 1
)

echo ============================================================
echo  Nexus ADB Watchdog - version check
echo ============================================================
adb devices
echo.
echo --- binary -h ---
adb shell "%REMOTE_DIR%/watchdog -h"
echo.
echo --- status first lines ---
adb shell "cat %REMOTE_DIR%/watchdog.status"
echo.
exit /b 0
