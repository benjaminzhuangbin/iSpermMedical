# Nexus ADB Watchdog 2.3 - PowerShell installer (LF-safe)
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\install.ps1

$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot

$Remote = "/data/local/watchdog"
$Binary = Join-Path $PSScriptRoot "release\watchdog"
$Conf   = Join-Path $PSScriptRoot "release\watchdog.conf"

Write-Host "============================================================"
Write-Host " Nexus ADB Watchdog 2.3 - install.ps1"
Write-Host "============================================================"
Write-Host " Script dir : $PSScriptRoot"
Write-Host " Local bin  : $Binary"
Write-Host " Remote dir : $Remote"
Write-Host "============================================================"

if (-not (Test-Path -LiteralPath $Binary)) {
    Write-Host "[ERROR] Missing binary: $Binary"
    Write-Host "Copy the whole NexusADBWatchdog-2.3 folder including release\watchdog"
    exit 1
}
if (-not (Test-Path -LiteralPath $Conf)) {
    $src = Join-Path $PSScriptRoot "watchdog.conf"
    if (Test-Path -LiteralPath $src) { Copy-Item -Force $src $Conf }
}

$adb = Get-Command adb -ErrorAction SilentlyContinue
if (-not $adb) {
    Write-Host "[ERROR] adb not found in PATH"
    exit 1
}

& adb devices
Write-Host ""
Write-Host "[1/7] Stopping previous watchdog if running..."
& adb shell "if [ -f $Remote/watchdog.pid ]; then kill ``cat $Remote/watchdog.pid`` 2>/dev/null; fi"

Write-Host "[2/7] Creating remote directory..."
& adb shell "mkdir -p $Remote"

Write-Host "[3/7] Pushing watchdog binary..."
& adb push $Binary "$Remote/watchdog"
if ($LASTEXITCODE -ne 0) { Write-Host "[ERROR] push binary failed"; exit 1 }

Write-Host "[4/7] Pushing watchdog.conf..."
& adb push $Conf "$Remote/watchdog.conf"
if ($LASTEXITCODE -ne 0) { Write-Host "[ERROR] push conf failed"; exit 1 }

Write-Host "[5/7] Setting permissions..."
& adb shell "chmod 755 $Remote"
& adb shell "chmod 755 $Remote/watchdog"
& adb shell "chmod 644 $Remote/watchdog.conf"

Write-Host "[6/7] Creating placeholders (Android 5.1 safe)..."
& adb shell "cat /dev/null > $Remote/watchdog.log"
& adb shell "cat /dev/null > $Remote/watchdog.status"
& adb shell "cat /dev/null > $Remote/watchdog.pid"
& adb shell "chmod 666 $Remote/watchdog.log"
& adb shell "chmod 666 $Remote/watchdog.status"
& adb shell "chmod 644 $Remote/watchdog.pid"

Write-Host "[7/7] Verifying + version..."
& adb shell "ls -l $Remote"
Write-Host ""
& adb shell "$Remote/watchdog -h"
Write-Host ""
Write-Host "[OK] Install complete - Version 2.3"
Write-Host "Next: start.bat   or   .\start.bat"
