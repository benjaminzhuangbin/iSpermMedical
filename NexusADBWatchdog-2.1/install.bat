@echo off
REM ============================================================================
REM Nexus ADB Watchdog 2.1 - install.bat
REM Push binary + config to Android: /data/local/watchdog/
REM IMPORTANT: This file must use Windows CRLF line endings.
REM ============================================================================
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

set "REMOTE_DIR=/data/local/watchdog"
set "BINARY=%~dp0release\watchdog"
set "CONF=%~dp0release\watchdog.conf"

echo ============================================================
echo  Nexus ADB Watchdog 2.1 - install
echo ============================================================
echo  Script dir : %~dp0
echo  Local bin  : %BINARY%
echo  Remote dir : %REMOTE_DIR%
echo ============================================================

if not exist "%BINARY%" (
    echo [ERROR] Missing binary:
    echo   %BINARY%
    echo.
    echo Make sure you copied the whole NexusADBWatchdog-2.1 folder,
    echo including the release\watchdog file. Or run build.bat first.
    echo.
    pause
    exit /b 1
)

if not exist "%CONF%" (
    if exist "%~dp0watchdog.conf" (
        copy /Y "%~dp0watchdog.conf" "%CONF%" >nul
    ) else (
        echo [ERROR] Missing config: %CONF%
        pause
        exit /b 1
    )
)

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH.
    echo Put platform-tools in PATH, or run from a folder where adb.exe works.
    pause
    exit /b 1
)

echo.
adb devices
echo.

echo [1/7] Stopping previous watchdog if running...
adb shell "if [ -f %REMOTE_DIR%/watchdog.pid ]; then kill `cat %REMOTE_DIR%/watchdog.pid` 2>/dev/null; fi"

echo [2/7] Creating remote directory...
adb shell "mkdir -p %REMOTE_DIR%"
if errorlevel 1 (
    echo [ERROR] mkdir failed. Is the device connected?
    pause
    exit /b 1
)

echo [3/7] Pushing watchdog binary...
adb push "%BINARY%" "%REMOTE_DIR%/watchdog"
if errorlevel 1 (
    echo [ERROR] adb push binary failed.
    pause
    exit /b 1
)

echo [4/7] Pushing watchdog.conf...
adb push "%CONF%" "%REMOTE_DIR%/watchdog.conf"
if errorlevel 1 (
    echo [ERROR] adb push config failed.
    pause
    exit /b 1
)

echo [5/7] Setting permissions...
adb shell "chmod 755 %REMOTE_DIR%"
adb shell "chmod 755 %REMOTE_DIR%/watchdog"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.conf"

echo [6/7] Creating log/status placeholders (Android 5.1 safe)...
REM Android 5.1 toolbox touch often does NOT support multiple files.
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.log"
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.status"
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.pid"
adb shell "chmod 666 %REMOTE_DIR%/watchdog.log"
adb shell "chmod 666 %REMOTE_DIR%/watchdog.status"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.pid"

echo [7/7] Verifying install + version...
adb shell "ls -l %REMOTE_DIR%"
echo.
echo --- Binary help/version (from device) ---
adb shell "%REMOTE_DIR%/watchdog -h"
echo.

echo [OK] Install complete - Version 2.1
echo.
echo Next:
echo   start.bat
echo   status.bat
echo.
echo Or manually:
echo   adb shell "nohup %REMOTE_DIR%/watchdog >/dev/null 2>&1 ^&"
echo   adb shell "cat %REMOTE_DIR%/watchdog.status"
echo.
pause
exit /b 0
