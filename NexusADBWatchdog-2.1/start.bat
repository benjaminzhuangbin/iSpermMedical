@echo off
REM Start watchdog on Android (nohup)
setlocal
set "REMOTE=/data/local/watchdog"
where adb >nul 2>nul || (echo [ERROR] adb not in PATH & exit /b 1)
echo === Start Watchdog 2.1 ===
adb shell "if [ -f %REMOTE%/watchdog.pid ]; then kill `cat %REMOTE%/watchdog.pid` 2>/dev/null; sleep 1; fi"
adb shell "nohup %REMOTE%/watchdog >/dev/null 2>&1 &"
timeout /t 2 /nobreak >nul
adb shell "cat %REMOTE%/watchdog.pid 2>/dev/null"
adb shell "cat %REMOTE%/watchdog.status 2>/dev/null"
echo [OK] started
exit /b 0
