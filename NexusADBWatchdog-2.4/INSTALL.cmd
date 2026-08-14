@echo off
echo.
echo === INSTALL.cmd wrapper for Nexus ADB Watchdog 2.4 ===
echo Folder: %CD%
echo.
cd /d "%~dp0"
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
pause
exit /b %ERR%
