@echo off
REM Nexus ADB Watchdog 2.3 - build.bat
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

if not defined ANDROID_NDK_HOME (
    if defined ANDROID_NDK_ROOT set "ANDROID_NDK_HOME=%ANDROID_NDK_ROOT%"
)
if not defined ANDROID_NDK_HOME (
    if defined NDK_ROOT set "ANDROID_NDK_HOME=%NDK_ROOT%"
)
if not defined ANDROID_NDK_HOME (
    if exist "%LOCALAPPDATA%\Android\Sdk\ndk" (
        for /f "delims=" %%D in ('dir /b /ad /o-n "%LOCALAPPDATA%\Android\Sdk\ndk" 2^>nul') do (
            if not defined ANDROID_NDK_HOME set "ANDROID_NDK_HOME=%LOCALAPPDATA%\Android\Sdk\ndk\%%D"
        )
    )
)

if not defined ANDROID_NDK_HOME (
    echo [ERROR] Set ANDROID_NDK_HOME
    pause
    exit /b 1
)
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not in PATH
    pause
    exit /b 1
)

set "ABI=armeabi-v7a"
set "API_LEVEL=22"
set "BUILD_DIR=%~dp0build-android"
set "RELEASE_DIR=%~dp0release"

echo ============================================================
echo  Nexus ADB Watchdog 2.3 - NDK build
echo ============================================================

if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

cmake -S "%~dp0." -B "%BUILD_DIR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" ^
    -DANDROID_ABI=%ABI% ^
    -DANDROID_PLATFORM=android-%API_LEVEL% ^
    -DANDROID_NATIVE_API_LEVEL=%API_LEVEL% ^
    -DANDROID_STL=none ^
    -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [ERROR] cmake configure failed
    pause
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo [ERROR] build failed
    pause
    exit /b 1
)

if not exist "%RELEASE_DIR%\watchdog" (
    if exist "%BUILD_DIR%\watchdog" copy /Y "%BUILD_DIR%\watchdog" "%RELEASE_DIR%\watchdog" >nul
)
copy /Y "%~dp0watchdog.conf" "%RELEASE_DIR%\watchdog.conf" >nul

if not exist "%RELEASE_DIR%\watchdog" (
    echo [ERROR] binary missing
    pause
    exit /b 1
)

echo [OK] %RELEASE_DIR%\watchdog
pause
exit /b 0
