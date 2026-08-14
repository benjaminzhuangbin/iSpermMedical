@echo off
REM Nexus ADB Watchdog 2.3 - uninstall.bat
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
set "REMOTE_DIR=/data/local/watchdog"

where adb >nul 2>nul
if errorlevel 1 (
    echo [ERROR] adb not found in PATH.
    exit /b 1
)

echo ============================================================
echo  Nexus ADB Watchdog 2.3 - uninstall
echo ============================================================
adb devices

echo.
echo Stopping watchdog...
adb shell "if [ -f %REMOTE_DIR%/watchdog.pid ]; then kill `cat %REMOTE_DIR%/watchdog.pid` 2>/dev/null; sleep 1; fi"
echo Removing files...
adb shell "rm -f %REMOTE_DIR%/watchdog %REMOTE_DIR%/watchdog.conf %REMOTE_DIR%/watchdog.pid %REMOTE_DIR%/watchdog.status %REMOTE_DIR%/watchdog.log %REMOTE_DIR%/watchdog.log.1 %REMOTE_DIR%/watchdog.error %REMOTE_DIR%/watchdog.inject %REMOTE_DIR%/watchdog.status.tmp %REMOTE_DIR%/.adb_protocol_fault"
adb shell "rmdir %REMOTE_DIR% 2>/dev/null"
echo [OK] uninstall complete
exit /b 0
