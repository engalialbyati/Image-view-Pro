$ErrorActionPreference = 'Stop'

$root = $MyInvocation.MyCommand.Path
if ($root) { $root = Split-Path -Parent $root } else { $root = (Get-Location).Path }
Set-Location -LiteralPath $root
Write-Output "Project: $root"

$mingwBin = $null
$pkg = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
if (Test-Path $pkg) {
    $wl = Get-ChildItem $pkg -Filter '*WinLibs*' -Directory -ErrorAction SilentlyContinue
    foreach ($d in $wl) {
        $b = Join-Path $d.FullName 'mingw64\bin\g++.exe'
        if (Test-Path $b) { $mingwBin = Join-Path $d.FullName 'mingw64\bin'; break }
    }
}
if (-not $mingwBin -and (Get-Command g++ -ErrorAction SilentlyContinue)) { $mingwBin = $null }
if ($mingwBin) {
    Write-Output "Using MinGW: $mingwBin"
    $env:Path = "$mingwBin;" + $env:Path
}

$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gpp) { throw 'g++ not found. Install MinGW-w64 first.' }
& g++ --version | Select-Object -First 1

Write-Output 'Compiling resources...'
& windres 'app.rc' -O coff -o 'app.res'
if ($LASTEXITCODE -ne 0) { throw 'windres failed' }

$out = 'Image Viewer Pro.exe'
Write-Output 'Compiling...'
& g++ -O2 -std=c++17 -municode -mwindows -static `
    -DUNICODE -D_UNICODE `
    -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 `
    'main.cpp' 'app.res' `
    -o $out `
    -lgdiplus -lcomctl32 -lole32 -loleaut32 -luuid `
    -lshell32 -lshlwapi -luser32 -lgdi32 -lcomdlg32 -lmsimg32 -ldwmapi -lwindowscodecs
if ($LASTEXITCODE -ne 0) { throw 'g++ compile failed' }

Remove-Item -LiteralPath 'app.res' -Force -ErrorAction SilentlyContinue
$full = Join-Path $root $out
$size = [math]::Round((Get-Item $full).Length / 1KB, 0)
Write-Output "OK -> $full  ($size KB)"
