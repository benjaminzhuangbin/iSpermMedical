@echo off
REM ============================================================================
REM Nexus ADB Watchdog - build.bat
REM Build a native ARMv7 executable for Android 5.1.1 (RK3288)
REM using Android NDK + CMake from the Windows command line.
REM ============================================================================
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

REM ---- Locate Android NDK -------------------------------------------------
REM Priority:
REM   1) ANDROID_NDK_HOME
REM   2) ANDROID_NDK_ROOT
REM   3) NDK_ROOT
REM   4) Common Windows install locations

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
    if exist "%USERPROFILE%\AppData\Local\Android\Sdk\ndk" (
        for /f "delims=" %%D in ('dir /b /ad /o-n "%USERPROFILE%\AppData\Local\Android\Sdk\ndk" 2^>nul') do (
            if not defined ANDROID_NDK_HOME set "ANDROID_NDK_HOME=%USERPROFILE%\AppData\Local\Android\Sdk\ndk\%%D"
        )
    )
)

if not defined ANDROID_NDK_HOME (
    echo [ERROR] Android NDK not found.
    echo Set ANDROID_NDK_HOME to your NDK path, for example:
    echo   set ANDROID_NDK_HOME=C:\Android\Sdk\ndk\21.4.7075529
    exit /b 1
)

if not exist "%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" (
    echo [ERROR] Invalid NDK path: %ANDROID_NDK_HOME%
    echo Missing: build\cmake\android.toolchain.cmake
    exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH.
    echo Install CMake and ensure "cmake" is available from cmd.exe
    exit /b 1
)

REM ---- Build options (Android 5.1.1 = API 22, ARMv7) ----------------------
set "ABI=armeabi-v7a"
set "API_LEVEL=22"
set "BUILD_DIR=%~dp0build-android"
set "RELEASE_DIR=%~dp0release"

echo ============================================================
echo  Nexus ADB Watchdog - Android NDK build
echo ============================================================
echo  NDK       : %ANDROID_NDK_HOME%
echo  ABI       : %ABI%
echo  API       : %API_LEVEL%
echo  Output    : %RELEASE_DIR%\watchdog
echo ============================================================

if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

cmake -S "%~dp0." -B "%BUILD_DIR%" ^
    -G "Ninja" ^
    -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" ^
    -DANDROID_ABI=%ABI% ^
    -DANDROID_PLATFORM=android-%API_LEVEL% ^
    -DANDROID_NATIVE_API_LEVEL=%API_LEVEL% ^
    -DANDROID_STL=none ^
    -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo.
    echo [WARN] Ninja generator failed; retrying with "Unix Makefiles" / MinGW Makefiles...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    mkdir "%BUILD_DIR%"

    cmake -S "%~dp0." -B "%BUILD_DIR%" ^
        -G "MinGW Makefiles" ^
        -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" ^
        -DANDROID_ABI=%ABI% ^
        -DANDROID_PLATFORM=android-%API_LEVEL% ^
        -DANDROID_NATIVE_API_LEVEL=%API_LEVEL% ^
        -DANDROID_STL=none ^
        -DCMAKE_BUILD_TYPE=Release

    if errorlevel 1 (
        echo.
        echo [WARN] MinGW Makefiles failed; trying default generator...
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
            echo [ERROR] CMake configure failed.
            exit /b 1
        )
    )
)

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

if not exist "%RELEASE_DIR%\watchdog" (
    REM Some generators place the binary under build tree; copy if needed.
    if exist "%BUILD_DIR%\watchdog" copy /Y "%BUILD_DIR%\watchdog" "%RELEASE_DIR%\watchdog" >nul
    if exist "%BUILD_DIR%\Release\watchdog" copy /Y "%BUILD_DIR%\Release\watchdog" "%RELEASE_DIR%\watchdog" >nul
)

if not exist "%RELEASE_DIR%\watchdog.conf" (
    copy /Y "%~dp0watchdog.conf" "%RELEASE_DIR%\watchdog.conf" >nul
)

if not exist "%RELEASE_DIR%\watchdog" (
    echo [ERROR] Binary not found in release\
    exit /b 1
)

echo.
echo [OK] Build complete.
echo     Binary : %RELEASE_DIR%\watchdog
echo     Config : %RELEASE_DIR%\watchdog.conf
echo.
echo Next: run install.bat  (requires adb connected to the device)
exit /b 0
