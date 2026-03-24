# MediaDownloader Smart Binary Downloader
# Windows version - updates binaries in bin/windows/
# [2026-03-19]
param([switch]$Force)
$ErrorActionPreference = "Stop"

$PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
$WindowsDir = Join-Path $PSScriptRoot "bin\windows"
$TempDir = Join-Path $env:TEMP "MediaDownloader_Updates"

if (-not (Test-Path $WindowsDir)) { New-Item -ItemType Directory -Path $WindowsDir -Force | Out-Null }
if (-not (Test-Path $TempDir)) { New-Item -ItemType Directory -Path $TempDir -Force | Out-Null }

Write-Host "`n=== MediaDownloader Smart Binary Downloader ===" -ForegroundColor Yellow
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13

# --- Helper: Get Local Version ---
function Get-LocalVersion {
    param($name)
    $exe = Join-Path $WindowsDir "$name.exe"
    if (-not (Test-Path $exe)) { return $null }
    try {
        if ($name -eq 'ffmpeg') {
            $line = (& $exe -version | Select-Object -First 1)
            if ($line -match "version\s+([^\s,]+)") { return $matches[1] }
        }
        elseif ($name -eq 'deno') {
            $line = (& $exe --version | Select-Object -First 1)
            return ($line -split ' ')[1]
        }
        elseif ($name -eq 'aria2c') {
            $output = & $exe --version
            if ($output -join "`n" -match "aria2 version\s+([0-9.]+)") { return $matches[1] }
        }
        else {
            return (& $exe --version).Trim()
        }
    } catch { return 'BROKEN' }
    return 'UNKNOWN'
}

# --- Helper: Get Remote GitHub Tag ---
function Get-RemoteGHVersion {
    param($repo)
    try {
        $api = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -Headers @{'User-Agent'='PS'}
        $tag = $api.tag_name
        # Clean up common prefixes like 'v' or 'release-' to match binary output
        $tag = $tag.TrimStart('v')
        $tag = $tag -replace '^release-', ''
        return $tag
    } catch { return $null }
}

$allSuccess = $true

# 1. yt-dlp (Nightly Build)
Write-Host "Checking yt-dlp (Nightly)... " -NoNewline -ForegroundColor Cyan
$ytLocal = Get-LocalVersion 'yt-dlp'
$ytRemote = Get-RemoteGHVersion 'yt-dlp/yt-dlp-nightly-builds'
if ($ytLocal -eq $ytRemote -and $ytLocal -ne 'BROKEN' -and -not $Force) {
    Write-Host "Up to date ($ytLocal)" -ForegroundColor Green
} else {
    Write-Host "Updating ($ytLocal -> $ytRemote)..." -ForegroundColor Yellow
    try {
        Invoke-WebRequest -Uri "https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest/download/yt-dlp.exe" -OutFile (Join-Path $WindowsDir "yt-dlp.exe") -UseBasicParsing
        Write-Host "[OK]" -ForegroundColor Green
    } catch { $allSuccess = $false; Write-Host "[FAIL]" -ForegroundColor Red }
}

# 2. gallery-dl
Write-Host "Checking gallery-dl... " -NoNewline -ForegroundColor Cyan
$gLocal = Get-LocalVersion 'gallery-dl'
$gRemote = Get-RemoteGHVersion 'mikf/gallery-dl'
if ($gLocal -eq $gRemote -and $gLocal -ne 'BROKEN' -and -not $Force) {
    Write-Host "Up to date ($gLocal)" -ForegroundColor Green
} else {
    Write-Host "Updating ($gLocal -> $gRemote)..." -ForegroundColor Yellow
    try {
        Invoke-WebRequest -Uri "https://github.com/mikf/gallery-dl/releases/latest/download/gallery-dl.exe" -OutFile (Join-Path $WindowsDir "gallery-dl.exe") -UseBasicParsing
        Write-Host "[OK]" -ForegroundColor Green
    } catch { $allSuccess = $false; Write-Host "[FAIL]" -ForegroundColor Red }
}

# 3. FFmpeg
Write-Host "Checking FFmpeg... " -NoNewline -ForegroundColor Cyan
$ffLocal = Get-LocalVersion 'ffmpeg'
if ($null -eq $ffLocal -or $ffLocal -eq 'BROKEN' -or $Force) {
    Write-Host "Updating..." -ForegroundColor Yellow
    try {
        $ffZip = Join-Path $TempDir 'ff.zip'; $ffEx = Join-Path $TempDir 'ff_tmp'
        if (Test-Path $ffEx) { Remove-Item $ffEx -Recurse -Force }
        Import-Module BitsTransfer
        Start-BitsTransfer -Source 'https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip' -Destination $ffZip
        Expand-Archive -Path $ffZip -DestinationPath $ffEx -Force
        $bin = Get-ChildItem -Path $ffEx -Recurse -Filter 'ffmpeg.exe' | Select-Object -ExpandProperty FullName -First 1
        $probe = Get-ChildItem -Path $ffEx -Recurse -Filter 'ffprobe.exe' | Select-Object -ExpandProperty FullName -First 1
        Copy-Item $bin (Join-Path $WindowsDir 'ffmpeg.exe') -Force
        Copy-Item $probe (Join-Path $WindowsDir 'ffprobe.exe') -Force
        Write-Host "[OK]" -ForegroundColor Green
    } catch { $allSuccess = $false; Write-Host "[FAIL]" -ForegroundColor Red }
} else {
    Write-Host "Up to date ($ffLocal)" -ForegroundColor Green
}

# 4. aria2c
Write-Host "Checking aria2c... " -NoNewline -ForegroundColor Cyan
$aLocal = Get-LocalVersion 'aria2c'
$aRemote = Get-RemoteGHVersion 'aria2/aria2'
if ($aLocal -eq $aRemote -and $aLocal -ne 'BROKEN' -and -not $Force) {
    Write-Host "Up to date ($aLocal)" -ForegroundColor Green
} else {
    Write-Host "Updating ($aLocal -> $aRemote)..." -ForegroundColor Yellow
    try {
        $api = Invoke-RestMethod -Uri "https://api.github.com/repos/aria2/aria2/releases/latest" -Headers @{'User-Agent'='PS'}
        $asset = $api.assets | Where-Object { $_.name -like "aria2*-win-64bit-build1.zip" } | Select-Object -First 1
        $aZip = Join-Path $TempDir 'aria.zip'
        Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $aZip -UseBasicParsing
        Expand-Archive -Path $aZip -DestinationPath $TempDir -Force
        $exe = Get-ChildItem -Path $TempDir -Recurse -Filter "aria2c.exe" | Select-Object -ExpandProperty FullName -First 1
        Copy-Item $exe (Join-Path $WindowsDir "aria2c.exe") -Force
        Write-Host "[OK]" -ForegroundColor Green
    } catch { $allSuccess = $false; Write-Host "[FAIL]" -ForegroundColor Red }
}

# 5. Deno
Write-Host "Checking Deno... " -NoNewline -ForegroundColor Cyan
$dLocal = Get-LocalVersion 'deno'
$dRemote = Get-RemoteGHVersion 'denoland/deno'
if ($dLocal -eq $dRemote -and $dLocal -ne 'BROKEN' -and -not $Force) {
    Write-Host "Up to date ($dLocal)" -ForegroundColor Green
} else {
    Write-Host "Updating..." -ForegroundColor Yellow
    try {
        $dZip = Join-Path $TempDir 'deno.zip'
        Invoke-WebRequest -Uri 'https://github.com/denoland/deno/releases/latest/download/deno-x86_64-pc-windows-msvc.zip' -OutFile $dZip -UseBasicParsing
        Expand-Archive -Path $dZip -DestinationPath $TempDir -Force
        Copy-Item (Join-Path $TempDir 'deno.exe') (Join-Path $WindowsDir 'deno.exe') -Force
        Write-Host "[OK]" -ForegroundColor Green
    } catch { $allSuccess = $false; Write-Host "[FAIL]" -ForegroundColor Red }
}

if (Test-Path $TempDir) { Remove-Item $TempDir -Recurse -Force | Out-Null }
Write-Host "`nFinished. Press any key to exit..."
$null = [Console]::ReadKey()