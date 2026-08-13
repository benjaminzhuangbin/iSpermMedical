@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "REMOTE=/data/local/watchdog"
echo ============================================================
echo  Nexus ADB Watchdog 2.2 - uninstall
echo ============================================================
where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not in PATH
    pause
    exit /b 1
)
adb devices
adb shell "if [ -f %REMOTE%/watchdog.pid ]; then kill `cat %REMOTE%/watchdog.pid` 2>/dev/null; sleep 1; fi"
adb shell "rm -f %REMOTE%/watchdog %REMOTE%/watchdog.conf %REMOTE%/watchdog.pid %REMOTE%/watchdog.status %REMOTE%/watchdog.log %REMOTE%/watchdog.log.1 %REMOTE%/watchdog.error %REMOTE%/watchdog.inject %REMOTE%/watchdog.status.tmp"
adb shell "rmdir %REMOTE% 2>/dev/null"
echo [OK] uninstall complete
pause
exit /b 0
