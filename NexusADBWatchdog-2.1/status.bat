@echo off
REM Show watchdog status / recent log from Android
setlocal
set "REMOTE=/data/local/watchdog"
where adb >nul 2>nul || (echo [ERROR] adb not in PATH & exit /b 1)
echo === watchdog.status ===
adb shell "cat %REMOTE%/watchdog.status"
echo.
echo === watchdog.pid ===
adb shell "cat %REMOTE%/watchdog.pid 2>/dev/null"
echo.
echo === last log lines ===
adb shell "cat %REMOTE%/watchdog.log 2>/dev/null" | more
exit /b 0
