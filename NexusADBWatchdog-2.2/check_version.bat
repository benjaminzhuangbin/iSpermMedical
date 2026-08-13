@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "REMOTE=/data/local/watchdog"
echo ============================================================
echo  Which Watchdog version is on Android?
echo ============================================================
where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not in PATH
    pause
    exit /b 1
)
adb devices
echo.
echo [1] Binary -h :
adb shell "%REMOTE%/watchdog -h"
echo.
echo [2] First line of status (should be version banner):
adb shell "cat %REMOTE%/watchdog.status"
echo.
echo [3] Log start markers:
adb shell "cat %REMOTE%/watchdog.log" 2>nul | findstr /i "started"
echo.
pause
exit /b 0
