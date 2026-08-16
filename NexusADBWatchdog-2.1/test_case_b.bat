@echo off
REM TEST inject CASE B: break TCP listen; watchdog should RESTART_ADBD
setlocal
set "REMOTE=/data/local/watchdog"
where adb >nul 2>nul || exit /b 1
echo Writing inject BREAK_PORT ...
adb shell "echo BREAK_PORT > %REMOTE%/watchdog.inject"
echo Wait ~15s then run status.bat
echo Expected: PORT5555=YES after recovery
exit /b 0
