@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo Writing CLEAR_TCP_PORT inject (CASE C prop)...
adb shell "echo CLEAR_TCP_PORT > /data/local/watchdog/watchdog.inject"
echo Wait ~15s then status.bat
echo Expected: PROP_OK=1 TCP_HEALTH=OK or NO_CLIENT
pause
exit /b 0
