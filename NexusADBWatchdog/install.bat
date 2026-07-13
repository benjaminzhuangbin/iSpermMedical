@echo off
REM ============================================================================
REM Nexus ADB Watchdog - install.bat
REM Push binary + config to Android device and set permissions.
REM Install path: /data/local/watchdog/
REM ============================================================================
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

set "RELEASE_DIR=%~dp0release"
set "REMOTE_DIR=/data/local/watchdog"
set "BINARY=%RELEASE_DIR%\watchdog"
set "CONF=%RELEASE_DIR%\watchdog.conf"

if not exist "%BINARY%" (
    echo [ERROR] Missing binary: %BINARY%
    echo Run build.bat first.
    exit /b 1
)

if not exist "%CONF%" (
    if exist "%~dp0watchdog.conf" (
        copy /Y "%~dp0watchdog.conf" "%CONF%" >nul
    ) else (
        echo [ERROR] Missing config: %CONF%
        exit /b 1
    )
)

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH.
    exit /b 1
)

echo ============================================================
echo  Nexus ADB Watchdog - install
echo ============================================================
echo  Local binary : %BINARY%
echo  Remote dir   : %REMOTE_DIR%
echo ============================================================

REM Prefer TCP device if already connected; otherwise use default adb device.
adb devices

echo.
echo [1/5] Creating remote directory...
adb shell "mkdir -p %REMOTE_DIR%"
if errorlevel 1 (
    echo [ERROR] mkdir failed. Is the device connected / rooted path writable?
    exit /b 1
)

echo [2/5] Pushing watchdog binary...
adb push "%BINARY%" "%REMOTE_DIR%/watchdog"
if errorlevel 1 (
    echo [ERROR] adb push binary failed.
    exit /b 1
)

echo [3/5] Pushing watchdog.conf...
adb push "%CONF%" "%REMOTE_DIR%/watchdog.conf"
if errorlevel 1 (
    echo [ERROR] adb push config failed.
    exit /b 1
)

echo [4/5] Setting permissions...
adb shell "chmod 755 %REMOTE_DIR%"
adb shell "chmod 755 %REMOTE_DIR%/watchdog"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.conf"
adb shell "touch %REMOTE_DIR%/watchdog.log %REMOTE_DIR%/watchdog.status %REMOTE_DIR%/watchdog.pid"
adb shell "chmod 666 %REMOTE_DIR%/watchdog.log %REMOTE_DIR%/watchdog.status"
adb shell "chmod 644 %REMOTE_DIR%/watchdog.pid"

echo [5/5] Verifying remote files...
adb shell "ls -l %REMOTE_DIR%"

echo.
echo [OK] Install complete.
echo.
echo Start the daemon on the device:
echo   adb shell "nohup %REMOTE_DIR%/watchdog ^> /dev/null 2^>^&1 ^&"
echo.
echo Or interactively:
echo   adb shell
echo   nohup /data/local/watchdog/watchdog ^&
echo.
echo Check status:
echo   adb shell "cat %REMOTE_DIR%/watchdog.status"
echo   adb shell "tail -n 40 %REMOTE_DIR%/watchdog.log"
echo.
exit /b 0
