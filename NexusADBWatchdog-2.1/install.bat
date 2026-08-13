@echo off
REM Install watchdog to Android /data/local/watchdog/
setlocal
cd /d "%~dp0"
set "REMOTE=/data/local/watchdog"
set "BIN=%~dp0release\watchdog"
set "CONF=%~dp0release\watchdog.conf"
if not exist "%BIN%" (
    echo [ERROR] missing %BIN% - run build.bat
    exit /b 1
)
if not exist "%CONF%" copy /Y "%~dp0watchdog.conf" "%CONF%" >nul
where adb >nul 2>nul || (echo [ERROR] adb not in PATH & exit /b 1)

echo === Nexus ADB Watchdog 2.1 install ===
adb devices
adb shell "if [ -f %REMOTE%/watchdog.pid ]; then kill `cat %REMOTE%/watchdog.pid` 2>/dev/null; fi"
adb shell "mkdir -p %REMOTE%"
adb push "%BIN%" "%REMOTE%/watchdog"
adb push "%CONF%" "%REMOTE%/watchdog.conf"
adb shell "chmod 755 %REMOTE% %REMOTE%/watchdog"
adb shell "chmod 644 %REMOTE%/watchdog.conf"
adb shell "touch %REMOTE%/watchdog.log %REMOTE%/watchdog.status %REMOTE%/watchdog.pid"
adb shell "chmod 666 %REMOTE%/watchdog.log %REMOTE%/watchdog.status"
adb shell "ls -l %REMOTE%"
echo.
echo [OK] Installed on Android: %REMOTE%
echo Next: start.bat
exit /b 0
