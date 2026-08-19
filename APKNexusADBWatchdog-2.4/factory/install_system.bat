@echo off
REM factory/install_system.bat — Windows one-shot factory install (CRLF)
REM Requires: adb, device reachable, shell can "su -c".
REM Hospital users never run this — manufacturing / engineering only.

setlocal
cd /d "%~dp0\.."

if not exist "factory\nexus_su" (
  echo ERROR: factory\nexus_su missing. Build native helper first.
  exit /b 1
)
if not exist "release\NexusADBWatchdog.apk" (
  if exist "app\build\outputs\apk\debug\app-debug.apk" (
    if not exist "release" mkdir "release"
    copy /Y "app\build\outputs\apk\debug\app-debug.apk" "release\NexusADBWatchdog.apk" >nul
  ) else (
    echo [info] Building APK from gradle...
    call gradlew.bat assembleDebug
    if exist "app\build\outputs\apk\debug\app-debug.apk" (
      if not exist "release" mkdir "release"
      copy /Y "app\build\outputs\apk\debug\app-debug.apk" "release\NexusADBWatchdog.apk" >nul
    )
  )
)

echo [push] nexus_su + APK + install script
adb push factory\nexus_su /data/local/tmp/nexus_su
if exist "release\NexusADBWatchdog.apk" (
  adb push release\NexusADBWatchdog.apk /data/local/tmp/NexusADBWatchdog.apk
) else if exist "app\build\outputs\apk\debug\app-debug.apk" (
  adb push app\build\outputs\apk\debug\app-debug.apk /data/local/tmp/NexusADBWatchdog.apk
)
adb push factory\install_on_device.sh /data/local/tmp/install_on_device.sh
adb shell "chmod 755 /data/local/tmp/install_on_device.sh"
if errorlevel 1 (
  echo ERROR: adb push failed
  exit /b 1
)

echo [run] su factory install
adb shell su 0 /system/bin/sh /data/local/tmp/install_on_device.sh || adb shell su -c "/system/bin/sh /data/local/tmp/install_on_device.sh"
if errorlevel 1 (
  echo ERROR: factory install failed
  exit /b 1
)

echo.
echo Reboot now? Recommended.
echo   adb reboot
echo After reboot verify:
echo   adb shell "cat /sdcard/NexusADBWatchdog/watchdog.status"
echo Expect ROOT_OK=1 ROOT_UID=0 ROOT_METHOD=NEXUS_SU:/system/xbin/nexus_su
endlocal
