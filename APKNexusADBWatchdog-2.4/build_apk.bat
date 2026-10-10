@echo off
REM build_apk.bat — One-click build for NexusADBWatchdog 2.4 (Android 5.1.1)
setlocal
cd /d "%~dp0"

echo ========================================================
echo   Building NexusADBWatchdog 2.4 APK (Android 5.1.1)
echo ========================================================

call gradlew.bat assembleDebug

if not exist "release" mkdir "release"

if exist "app\build\outputs\apk\debug\app-debug.apk" (
    copy /Y "app\build\outputs\apk\debug\app-debug.apk" "release\NexusADBWatchdog.apk" >nul
    echo.
    echo [OK] Build Succeeded!
    echo Output APK: release\NexusADBWatchdog.apk
    echo.
    echo Next Step:
    echo   Run: factory\install_system.bat
) else (
    echo.
    echo [ERROR] Build failed! Check Gradle logs above.
)
