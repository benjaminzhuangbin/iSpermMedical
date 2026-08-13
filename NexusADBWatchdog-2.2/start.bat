@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "REMOTE=/data/local/watchdog"
echo ============================================================
echo  Nexus ADB Watchdog 2.2 - start
echo ============================================================
where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not in PATH
    pause
    exit /b 1
)
adb devices
adb shell "if [ -f %REMOTE%/watchdog.pid ]; then kill `cat %REMOTE%/watchdog.pid` 2>/dev/null; fi"
adb shell "sleep 1"
adb shell "nohup %REMOTE%/watchdog >/dev/null 2>&1 &"
adb shell "sleep 2"
echo PID:
adb shell "cat %REMOTE%/watchdog.pid 2>/dev/null"
echo.
echo Status first lines:
adb shell "cat %REMOTE%/watchdog.status 2>/dev/null"
echo.
echo [OK] started
pause
exit /b 0
