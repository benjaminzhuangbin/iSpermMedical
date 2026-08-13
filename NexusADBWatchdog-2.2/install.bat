@echo off
REM ============================================================================
REM Nexus ADB Watchdog 2.2 - install.bat
REM MUST be CRLF. If this script prints nothing under cmd.exe, your copy
REM still has Unix LF endings. Use INSTALL.cmd or install.ps1 instead.
REM ============================================================================

REM Print immediately (even if later lines fail to parse)
echo ============================================================
echo  Nexus ADB Watchdog 2.2 - install
echo ============================================================

setlocal
cd /d "%~dp0"
if errorlevel 1 (
    echo [ERROR] cannot cd to script directory
    pause
    exit /b 1
)

set REMOTE_DIR=/data/local/watchdog
set BINARY=%~dp0release\watchdog
set CONF=%~dp0release\watchdog.conf

echo Script dir : %~dp0
echo Local bin  : %BINARY%
echo Remote dir : %REMOTE_DIR%
echo.

if not exist "%BINARY%" goto ERR_NO_BIN
if not exist "%CONF%" (
    if exist "%~dp0watchdog.conf" copy /Y "%~dp0watchdog.conf" "%CONF%" >nul
)

where adb >nul 2>nul
if errorlevel 1 goto ERR_NO_ADB

echo.
adb devices
echo.

echo [1/7] Stopping previous watchdog if running...
adb shell "if [ -f %REMOTE_DIR%/watchdog.pid ]; then kill `cat %REMOTE_DIR%/watchdog.pid` 2>/dev/null; fi"

echo [2/7] Creating remote directory...
adb shell "mkdir -p %REMOTE_DIR%"
if errorlevel 1 goto ERR_MKDIR

echo [3/7] Pushing watchdog binary...
adb push "%BINARY%" "%REMOTE_DIR%/watchdog"
if errorlevel 1 goto ERR_PUSH_BIN

echo [4/7] Pushing watchdog.conf...
adb push "%CONF%" "%REMOTE_DIR%/watchdog.conf"
if errorlevel 1 goto ERR_PUSH_CONF

echo [5/7] Setting permissions...
adb shell "chmod 755 %REMOTE_DIR%"
adb shell "chmod 755 %REMOTE_DIR%/watchdog"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.conf"

echo [6/7] Creating placeholders (Android 5.1 safe)...
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.log"
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.status"
adb shell "cat /dev/null > %REMOTE_DIR%/watchdog.pid"
adb shell "chmod 666 %REMOTE_DIR%/watchdog.log"
adb shell "chmod 666 %REMOTE_DIR%/watchdog.status"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.pid"

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

:ERR_NO_BIN
echo [ERROR] Missing binary:
echo   %BINARY%
echo Copy the whole NexusADBWatchdog-2.2 folder including release\watchdog
pause
exit /b 1

:ERR_NO_ADB
echo [ERROR] adb not found in PATH
pause
exit /b 1

:ERR_MKDIR
echo [ERROR] mkdir failed - is device connected?
pause
exit /b 1

:ERR_PUSH_BIN
echo [ERROR] adb push binary failed
pause
exit /b 1

:ERR_PUSH_CONF
echo [ERROR] adb push config failed
pause
exit /b 1
