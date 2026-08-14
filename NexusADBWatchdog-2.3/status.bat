@echo off
REM Nexus ADB Watchdog 2.3 - status.bat
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
set "REMOTE_DIR=/data/local/watchdog"

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH.
    exit /b 1
)

echo ============================================================
echo  Nexus ADB Watchdog 2.3 - status
echo ============================================================
echo.
echo --- version ---
adb shell "%REMOTE_DIR%/watchdog -h"
echo.
echo --- watchdog.status ---
adb shell "cat %REMOTE_DIR%/watchdog.status"
echo.
echo --- watchdog.pid ---
adb shell "cat %REMOTE_DIR%/watchdog.pid 2>/dev/null"
echo.
exit /b 0
