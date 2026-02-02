# MediaDownloader Binary Dependencies Downloader
# Windows version - uses pre-existing ffmpeg in bin/windows/ffmpeg-8.0.1-essentials_build
param([switch]$Force)
$ErrorActionPreference = "Continue"
$BinDir = "$PSScriptRoot\bin"
$WindowsDir = "$BinDir\windows"

function Download-File {
    param([string]$Url, [string]$OutPath)
    if ((Test-Path $OutPath) -and -not $Force) {
        Write-Host "✓ Already exists: $(Split-Path $OutPath -Leaf)" -ForegroundColor Green
        return $true
    }
    $parentDir = Split-Path $OutPath -Parent
    if (-not (Test-Path $parentDir)) {
        New-Item -ItemType Directory -Path $parentDir -Force | Out-Null
    }
    Write-Host "Downloading: $Url" -ForegroundColor Cyan
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $Url -OutFile $OutPath -UseBasicParsing
        Write-Host "✓ Downloaded: $(Split-Path $OutPath -Leaf)" -ForegroundColor Green
        return $true
    } catch {
        Write-Host "✗ Failed to download: $Url`n  Error: $_" -ForegroundColor Red
        return $false
    }
}

Write-Host "`n=== MediaDownloader Binary Downloader ===" -ForegroundColor Yellow
Write-Host "Ensuring all required binaries are present.`n"

$allSuccess = $true

Write-Host "`n[Windows]" -ForegroundColor Yellow

# yt-dlp for Windows
$ytDlpUrl = "https://github.com/yt-dlp/yt-dlp/releases/download/2026.01.29/yt-dlp.exe"
if (-not (Download-File -Url $ytDlpUrl -OutPath "$WindowsDir\yt-dlp.exe")) {
    $allSuccess = $false
}

# Check if ffmpeg binaries exist in the pre-bundled directory
$ffmpegBuildDir = "$WindowsDir\ffmpeg-8.0.1-essentials_build\bin"
$ffmpegExe = Test-Path "$ffmpegBuildDir\ffmpeg.exe"
$ffprobeExe = Test-Path "$ffmpegBuildDir\ffprobe.exe"
if ($ffmpegExe -and $ffprobeExe) {
    Write-Host "✓ Found ffmpeg in bundled directory" -ForegroundColor Green
} else {
    Write-Host "⚠ ffmpeg.exe or ffprobe.exe not found in bundled directory" -ForegroundColor Yellow
    Write-Host "  Expected path: $ffmpegBuildDir" -ForegroundColor Yellow
}

if ($allSuccess) {
    Write-Host "`n✓ All required binaries are ready!" -ForegroundColor Green
    Write-Host "`nNext step: Run 'pyinstaller main.spec' to build the app." -ForegroundColor Cyan
    exit 0
} else {
    Write-Host "`n✗ Some downloads failed." -ForegroundColor Red
    exit 1
}
