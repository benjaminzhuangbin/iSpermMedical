@echo off
REM TEST inject CASE C: clear persist.adb.tcp.port
setlocal
set "REMOTE=/data/local/watchdog"
where adb >nul 2>nul || exit /b 1
echo Writing inject CLEAR_TCP_PORT ...
adb shell "echo CLEAR_TCP_PORT > %REMOTE%/watchdog.inject"
echo Wait ~15s then run status.bat
echo Expected: PROP_OK=1 PORT5555=YES
exit /b 0
