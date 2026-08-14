@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo Writing ADB_PROTOCOL_FAULT inject (CASE D)...
adb shell "echo ADB_PROTOCOL_FAULT > /data/local/watchdog/watchdog.inject"
echo Wait ~15s then status.bat
echo Expected: ADB_HEALTH=FAULT then FIX_ADB_PROTOCOL then ADB_HEALTH=OK
echo RESTART_COUNT should increase; RECOVERY_ENABLED stays 1
pause
exit /b 0
