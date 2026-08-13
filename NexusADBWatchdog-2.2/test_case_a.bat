@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo Writing STOP_ADBD inject (CASE A)...
adb shell "echo STOP_ADBD > /data/local/watchdog/watchdog.inject"
echo Wait ~10s then status.bat
echo Expected: ADBD=YES after START_ADBD
pause
exit /b 0
