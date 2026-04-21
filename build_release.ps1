$ErrorActionPreference = "Stop"

Write-Host "=== LzyDownloader Release Builder ===" -ForegroundColor Cyan

$cmakeContents = Get-Content CMakeLists.txt -Raw
$versionMatch = [regex]::Match($cmakeContents, 'project\(LzyDownloader VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)')
if (-not $versionMatch.Success) {
    throw "Could not determine app version from CMakeLists.txt"
}
$appVersion = $versionMatch.Groups[1].Value
Write-Host "Using app version $appVersion" -ForegroundColor Cyan

Write-Host "`n[0/4] Cleaning old build cache to prevent DLL mismatches..." -ForegroundColor Yellow
if (Test-Path build) { Remove-Item -Recurse -Force build }

Write-Host "`n[1/4] Updating Extractor Lists..." -ForegroundColor Yellow
"" | python ./update_yt-dlp_extractors.py
"" | python ./update_gallery-dl_extractors.py

Write-Host "`n[2/4] Configuring CMake (Release)..." -ForegroundColor Yellow
$VcpkgToolchain = "E:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$VcpkgToolchain"

Write-Host "`n[3/4] Building C++ Application..." -ForegroundColor Yellow
# This will compile the executable. 
# Note: Your CMakeLists.txt automatically runs windeployqt and copies the 
# extractor JSONs as a POST_BUILD step, so we don't need to do it manually here.
cmake --build build --config Release

Write-Host "`n[4/4] Building NSIS Installer..." -ForegroundColor Yellow
$nsisPath = "C:\Program Files (x86)\NSIS\makensis.exe"
if (Test-Path $nsisPath) {
    & $nsisPath "/DAPP_VERSION=$appVersion" LzyDownloader.nsi
    if ($LASTEXITCODE -ne 0) {
        Write-Error "NSIS compilation failed with exit code $LASTEXITCODE"
    }
} else {
    Write-Error "makensis.exe not found at $nsisPath. Please ensure NSIS is installed."
}

Write-Host "`n=== Build Complete! ===" -ForegroundColor Green
Write-Host "Your LzyDownloader-Setup installer should be ready in the current directory." -ForegroundColor Green
