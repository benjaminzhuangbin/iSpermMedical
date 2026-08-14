# Convert all .bat/.cmd in this folder to Windows CRLF
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\fix_crlf.ps1

Set-Location -LiteralPath $PSScriptRoot
$files = Get-ChildItem -Path $PSScriptRoot -File | Where-Object { $_.Extension -match '\.(bat|cmd)$' }

foreach ($f in $files) {
    $text = [System.IO.File]::ReadAllText($f.FullName)
    $text = $text -replace "`r`n", "`n" -replace "`r", "`n"
    $text = $text -replace "`n", "`r`n"
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($f.FullName, $text, $utf8NoBom)
    Write-Host "CRLF fixed: $($f.Name)"
}

Write-Host ""
Write-Host "Done. Now run:"
Write-Host "  INSTALL.cmd"
Write-Host "  or:  powershell -ExecutionPolicy Bypass -File .\install.ps1"
