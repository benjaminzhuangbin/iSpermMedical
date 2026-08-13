@echo off
REM Stop watchdog on Android
setlocal
set "REMOTE=/data/local/watchdog"
where adb >nul 2>nul || (echo [ERROR] adb not in PATH & exit /b 1)
echo === Stop Watchdog 2.1 ===
adb shell "if [ -f %REMOTE%/watchdog.pid ]; then kill `cat %REMOTE%/watchdog.pid` 2>/dev/null; sleep 1; fi"
adb shell "rm -f %REMOTE%/watchdog.pid"
echo [OK] stopped
exit /b 0
