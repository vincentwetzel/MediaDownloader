# MediaDownloader Binary Dependencies Downloader
# This script downloads yt-dlp and ffmpeg (all platforms) to bin/
# Run this before building with PyInstaller to ensure all dependencies are present.

param(
    [switch]$Force  # Re-download even if files exist
)

$ErrorActionPreference = "Stop"

$BinDir = "$PSScriptRoot\bin"
$WindowsDir = "$BinDir\windows"
$LinuxDir = "$BinDir\linux"
$MacOSDir = "$BinDir\macos"

function Download-File {
    param(
        [string]$Url,
        [string]$OutPath
    )
    
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
        # Use TLS 1.2+ for security
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13
        $ProgressPreference = 'SilentlyContinue'  # Suppress progress bar for cleaner output
        Invoke-WebRequest -Uri $Url -OutFile $OutPath -UseBasicParsing
        Write-Host "✓ Downloaded: $(Split-Path $OutPath -Leaf)" -ForegroundColor Green
        return $true
    } catch {
        Write-Host "✗ Failed to download: $Url`n  Error: $_" -ForegroundColor Red
        return $false
    }
}

function Extract-Zip {
    param(
        [string]$ZipPath,
        [string]$OutDir
    )
    
    Write-Host "Extracting: $(Split-Path $ZipPath -Leaf)" -ForegroundColor Cyan
    try {
        Expand-Archive -Path $ZipPath -DestinationPath $OutDir -Force
        Write-Host "✓ Extracted to: $OutDir" -ForegroundColor Green
        return $true
    } catch {
        Write-Host "✗ Failed to extract: $ZipPath`n  Error: $_" -ForegroundColor Red
        return $false
    }
}

Write-Host "`n=== MediaDownloader Binary Downloader ===" -ForegroundColor Yellow
Write-Host "This script downloads yt-dlp and ffmpeg for all platforms.`n"

# Create base directories
@($WindowsDir, $LinuxDir, $MacOSDir) | ForEach-Object {
    if (-not (Test-Path $_)) {
        New-Item -ItemType Directory -Path $_ -Force | Out-Null
    }
}

$allSuccess = $true

# ============================================================================
# Windows Binaries
# ============================================================================
Write-Host "`n[Windows]" -ForegroundColor Yellow

# yt-dlp for Windows
$ytDlpUrl = "https://github.com/yt-dlp/yt-dlp/releases/download/2026.01.29/yt-dlp.exe"
if (-not (Download-File -Url $ytDlpUrl -OutPath "$WindowsDir\yt-dlp.exe")) {
    $allSuccess = $false
}

# ffmpeg for Windows (using FFmpeg.org official release)
# Note: Using static build from ffmpeg.org or GitHub releases
$ffmpegZipUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-02-02-08-47/ffmpeg-N-127099-g77a88cb8fe-win64-gpl.zip"
$ffmpegZip = "$WindowsDir\ffmpeg_temp.zip"
if (Download-File -Url $ffmpegZipUrl -OutPath $ffmpegZip) {
    $ffmpegExtractDir = "$WindowsDir\ffmpeg_extract_temp"
    if (Extract-Zip -ZipPath $ffmpegZip -OutDir $ffmpegExtractDir) {
        # Copy only the ffmpeg and ffprobe executables to the Windows bin directory
        # The structure is usually: ffmpeg_N-...-win64-gpl/bin/{ffmpeg.exe, ffprobe.exe}
        $binExePath = Get-ChildItem -Path $ffmpegExtractDir -Filter "ffmpeg.exe" -Recurse | Select-Object -First 1
        if ($binExePath) {
            Copy-Item -Path $binExePath.FullName -Destination "$WindowsDir\ffmpeg.exe" -Force
            Write-Host "✓ Copied ffmpeg.exe" -ForegroundColor Green
        }
        
        $probePath = Get-ChildItem -Path $ffmpegExtractDir -Filter "ffprobe.exe" -Recurse | Select-Object -First 1
        if ($probePath) {
            Copy-Item -Path $probePath.FullName -Destination "$WindowsDir\ffprobe.exe" -Force
            Write-Host "✓ Copied ffprobe.exe" -ForegroundColor Green
        }
        
        Remove-Item -Path $ffmpegExtractDir -Recurse -Force
        Remove-Item -Path $ffmpegZip -Force
    } else {
        $allSuccess = $false
    }
} else {
    $allSuccess = $false
}

# ============================================================================
# Linux Binaries
# ============================================================================
Write-Host "`n[Linux]" -ForegroundColor Yellow

# yt-dlp for Linux
$ytDlpLinuxUrl = "https://github.com/yt-dlp/yt-dlp/releases/download/2026.01.29/yt-dlp"
if (-not (Download-File -Url $ytDlpLinuxUrl -OutPath "$LinuxDir\yt-dlp")) {
    $allSuccess = $false
}

# ffmpeg for Linux (static build)
$ffmpegLinuxUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-02-02-08-47/ffmpeg-N-127099-g77a88cb8fe-linux64-gpl.tar.xz"
$ffmpegLinuxTar = "$LinuxDir\ffmpeg_temp.tar.xz"
if (Download-File -Url $ffmpegLinuxUrl -OutPath $ffmpegLinuxTar) {
    Write-Host "Note: Linux tarball downloaded. Extract on Linux system before using." -ForegroundColor Yellow
    # For Windows systems, we'll just note that the tar.xz needs to be extracted on Linux
    # In practice, this can be extracted by the build process if tar is available
} else {
    $allSuccess = $false
}

# ============================================================================
# macOS Binaries
# ============================================================================
Write-Host "`n[macOS]" -ForegroundColor Yellow

# yt-dlp for macOS
$ytDlpMacUrl = "https://github.com/yt-dlp/yt-dlp/releases/download/2026.01.29/yt-dlp_macos"
if (-not (Download-File -Url $ytDlpMacUrl -OutPath "$MacOSDir\yt-dlp_macos")) {
    $allSuccess = $false
}

# ffmpeg for macOS (static/universal build)
$ffmpegMacUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-02-02-08-47/ffmpeg-N-127099-g77a88cb8fe-macos64-gpl.zip"
$ffmpegMacZip = "$MacOSDir\ffmpeg_temp.zip"
if (Download-File -Url $ffmpegMacUrl -OutPath $ffmpegMacZip) {
    $ffmpegMacExtractDir = "$MacOSDir\ffmpeg_extract_temp"
    if (Extract-Zip -ZipPath $ffmpegMacZip -OutDir $ffmpegMacExtractDir) {
        $binExePath = Get-ChildItem -Path $ffmpegMacExtractDir -Filter "ffmpeg" -Recurse | Select-Object -First 1
        if ($binExePath) {
            Copy-Item -Path $binExePath.FullName -Destination "$MacOSDir\ffmpeg" -Force
            Write-Host "✓ Copied ffmpeg" -ForegroundColor Green
        }
        
        $probePath = Get-ChildItem -Path $ffmpegMacExtractDir -Filter "ffprobe" -Recurse | Select-Object -First 1
        if ($probePath) {
            Copy-Item -Path $probePath.FullName -Destination "$MacOSDir\ffprobe" -Force
            Write-Host "✓ Copied ffprobe" -ForegroundColor Green
        }
        
        Remove-Item -Path $ffmpegMacExtractDir -Recurse -Force
        Remove-Item -Path $ffmpegMacZip -Force
    } else {
        $allSuccess = $false
    }
} else {
    $allSuccess = $false
}

# ============================================================================
# Summary
# ============================================================================
Write-Host "`n" -ForegroundColor Yellow
if ($allSuccess) {
    Write-Host "✓ All binaries downloaded successfully!" -ForegroundColor Green
    Write-Host "`nNext step: Run 'pyinstaller main.spec' to build the app." -ForegroundColor Cyan
    exit 0
} else {
    Write-Host "✗ Some downloads failed. Please check the errors above." -ForegroundColor Red
    Write-Host "You can retry with: .\download_binaries.ps1 -Force" -ForegroundColor Yellow
    exit 1
}
