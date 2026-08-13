@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo Writing BREAK_PORT inject (CASE B)...
adb shell "echo BREAK_PORT > /data/local/watchdog/watchdog.inject"
echo Wait ~15s then status.bat
echo Expected: PORT5555=YES after RESTART_ADBD
pause
exit /b 0
