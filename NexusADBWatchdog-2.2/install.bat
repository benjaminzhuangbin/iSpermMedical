@echo off
REM ============================================================================
REM Nexus ADB Watchdog 2.2 - install.bat (Windows CRLF required)
REM Installs to Android: /data/local/watchdog/
REM ============================================================================
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "REMOTE_DIR=/data/local/watchdog"
set "BINARY=%~dp0release\watchdog"
set "CONF=%~dp0release\watchdog.conf"

echo ============================================================
echo  Nexus ADB Watchdog 2.2 - install
echo ============================================================
echo  Script dir : %~dp0
echo  Local bin  : %BINARY%
echo  Remote dir : %REMOTE_DIR%
echo ============================================================

if not exist "%BINARY%" (
    echo [ERROR] Missing binary: %BINARY%
    echo Copy the whole folder including release\watchdog, or run build.bat
    pause
    exit /b 1
)
if not exist "%CONF%" (
    if exist "%~dp0watchdog.conf" copy /Y "%~dp0watchdog.conf" "%CONF%" >nul
)

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH
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
    echo [ERROR] mkdir failed
    pause
    exit /b 1
)

echo [3/7] Pushing watchdog binary...
adb push "%BINARY%" "%REMOTE_DIR%/watchdog"
if errorlevel 1 (
    echo [ERROR] push binary failed
    pause
    exit /b 1
)

echo [4/7] Pushing watchdog.conf...
adb push "%CONF%" "%REMOTE_DIR%/watchdog.conf"
if errorlevel 1 (
    echo [ERROR] push conf failed
    pause
    exit /b 1
)

echo [5/7] Setting permissions...
adb shell "chmod 755 %REMOTE_DIR%"
adb shell "chmod 755 %REMOTE_DIR%/watchdog"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.conf"

echo [6/7] Creating placeholders (Android 5.1 safe; missing log/status/pid is OK)...
REM Do NOT use multi-file touch (toolbox on 5.1 rejects it).
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.log"
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.status"
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.pid"
adb shell "chmod 666 %REMOTE_DIR%/watchdog.log %REMOTE_DIR%/watchdog.status 2>/dev/null"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.pid 2>/dev/null"

echo [7/7] Verifying + version...
adb shell "ls -l %REMOTE_DIR%"
echo.
adb shell "%REMOTE_DIR%/watchdog -h"
echo.
echo [OK] Install complete - Version 2.2
echo Next: start.bat   then   status.bat
echo.
pause
exit /b 0
