@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "REMOTE=/data/local/watchdog"
echo ============================================================
echo  Nexus ADB Watchdog 2.2 - stop
echo ============================================================
where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not in PATH
    pause
    exit /b 1
)
adb shell "if [ -f %REMOTE%/watchdog.pid ]; then kill `cat %REMOTE%/watchdog.pid` 2>/dev/null; sleep 1; fi"
adb shell "rm -f %REMOTE%/watchdog.pid"
echo [OK] stopped
pause
exit /b 0
