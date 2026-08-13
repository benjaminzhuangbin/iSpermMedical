@echo off
REM TEST inject CASE A: stop adbd; watchdog should START_ADBD
setlocal
set "REMOTE=/data/local/watchdog"
where adb >nul 2>nul || exit /b 1
echo Writing inject STOP_ADBD ...
adb shell "echo STOP_ADBD > %REMOTE%/watchdog.inject"
echo Wait ~10s then run status.bat
echo Expected: ADBD=YES PORT5555=YES LAST_ACTION=START_ADBD (then NONE)
exit /b 0
