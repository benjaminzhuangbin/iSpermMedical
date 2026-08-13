@echo off
REM Uninstall from Android device
setlocal
set "REMOTE=/data/local/watchdog"
where adb >nul 2>nul || (echo [ERROR] adb not in PATH & exit /b 1)
echo === Uninstall Watchdog 2.1 ===
adb shell "if [ -f %REMOTE%/watchdog.pid ]; then kill `cat %REMOTE%/watchdog.pid` 2>/dev/null; sleep 1; fi"
adb shell "rm -f %REMOTE%/watchdog %REMOTE%/watchdog.conf %REMOTE%/watchdog.pid %REMOTE%/watchdog.status %REMOTE%/watchdog.log %REMOTE%/watchdog.log.1 %REMOTE%/watchdog.error %REMOTE%/watchdog.inject %REMOTE%/watchdog.status.tmp"
adb shell "rmdir %REMOTE% 2>/dev/null"
echo [OK] Uninstall complete
exit /b 0
