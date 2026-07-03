$ErrorActionPreference = 'Stop'
$root = $MyInvocation.MyCommand.Path
if ($root) { $root = Split-Path -Parent $root } else { $root = (Get-Location).Path }
Set-Location -LiteralPath $root
Write-Output "Building app..."
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'build.ps1')

$mk = Get-ChildItem "C:\Program Files*\NSIS" -Filter "makensis.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $mk) { throw 'makensis not found. Install NSIS (winget install NSIS.NSIS).' }
Write-Output "Building installer with NSIS..."
& $mk.FullName (Join-Path $root 'installer.nsi')
if ($LASTEXITCODE -ne 0) { throw 'makensis failed' }

$out = Join-Path $root 'ImageViewerPro-Setup.exe'
if (Test-Path $out) {
    $size = [math]::Round((Get-Item $out).Length / 1KB, 0)
    Write-Output "OK -> $out  ($size KB)"
} else { throw 'installer not produced' }
