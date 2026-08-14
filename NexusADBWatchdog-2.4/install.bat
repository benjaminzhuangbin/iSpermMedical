@echo off
echo ============================================================
echo  Nexus ADB Watchdog 2.4 - install.bat STARTING
echo ============================================================
REM ============================================================================
REM Nexus ADB Watchdog 2.4 - install.bat
REM Push binary + config to /data/local/watchdog/
REM IMPORTANT: This file MUST use CRLF line endings for Windows cmd.exe
REM ============================================================================
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"
echo Working dir: %CD%

set "RELEASE_DIR=%~dp0release"
set "REMOTE_DIR=/data/local/watchdog"
set "BINARY=%RELEASE_DIR%\watchdog"
set "CONF=%RELEASE_DIR%\watchdog.conf"

if not exist "%BINARY%" (
    echo [ERROR] Missing binary: %BINARY%
    echo Run build.bat first, or copy release\watchdog into this folder.
    pause
    exit /b 1
)

if not exist "%CONF%" (
    copy /Y "%~dp0watchdog.conf" "%CONF%" >nul
)

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  Nexus ADB Watchdog 2.4 - install
echo ============================================================
adb devices

echo.
echo [1/6] Stopping previous watchdog if running...
adb shell "if [ -f %REMOTE_DIR%/watchdog.pid ]; then kill `cat %REMOTE_DIR%/watchdog.pid` 2>/dev/null; fi"

echo [2/6] Creating remote directory...
adb shell "mkdir -p %REMOTE_DIR%"
if errorlevel 1 (
    echo [ERROR] mkdir failed.
    pause
    exit /b 1
)

echo [3/6] Pushing watchdog binary...
adb push "%BINARY%" "%REMOTE_DIR%/watchdog"
if errorlevel 1 (
    echo [ERROR] push binary failed.
    pause
    exit /b 1
)

echo [4/6] Pushing watchdog.conf...
adb push "%CONF%" "%REMOTE_DIR%/watchdog.conf"
if errorlevel 1 (
    echo [ERROR] push conf failed.
    pause
    exit /b 1
)

echo [5/6] Setting permissions...
adb shell "chmod 755 %REMOTE_DIR%"
adb shell "chmod 755 %REMOTE_DIR%/watchdog"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.conf"
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.log"
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.status"
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.pid"
adb shell "chmod 666 %REMOTE_DIR%/watchdog.log"
adb shell "chmod 666 %REMOTE_DIR%/watchdog.status"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.pid"

echo [6/6] Verifying...
adb shell "ls -l %REMOTE_DIR%"
echo.
adb shell "%REMOTE_DIR%/watchdog -h"

echo.
echo [OK] Install complete - Version 2.4
echo.
echo Start:
echo   adb shell "nohup %REMOTE_DIR%/watchdog ^> /dev/null 2^>^&1 ^&"
echo.
echo Status / log:
echo   adb shell "cat %REMOTE_DIR%/watchdog.status"
echo   adb shell "cat %REMOTE_DIR%/watchdog.log"
echo.
echo NOTE: Version 2.4 - no CNXN probe. ESTABLISHED=0 is NOT a fault.
echo.
pause
exit /b 0
