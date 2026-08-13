@echo off
REM Wrapper: always shows something, then calls install.bat
cd /d "%~dp0"
echo.
echo === INSTALL.cmd wrapper for Nexus ADB Watchdog 2.2 ===
echo Folder: %CD%
echo.
if not exist "%~dp0install.bat" (
    echo [ERROR] install.bat not found next to INSTALL.cmd
    pause
    exit /b 1
)
if not exist "%~dp0release\watchdog" (
    echo [ERROR] release\watchdog missing
    echo Folder contents:
    dir /b
    pause
    exit /b 1
)
echo Calling install.bat ...
echo.
call "%~dp0install.bat"
set ERR=%ERRORLEVEL%
echo.
echo install.bat exit code: %ERR%
if not "%ERR%"=="0" (
    echo.
    echo If you saw NO install output above, your install.bat may still be Unix LF.
    echo Fix with PowerShell:
    echo   powershell -ExecutionPolicy Bypass -File "%~dp0fix_crlf.ps1"
    echo Then run:  powershell -ExecutionPolicy Bypass -File "%~dp0install.ps1"
)
pause
exit /b %ERR%
