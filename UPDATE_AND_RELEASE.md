# LzyDownloader C++ Update & Release Guide

This document describes how to build, package, and release the C++ version of LzyDownloader with auto-update support.

## Prerequisites

1. **NSIS (Nullsoft Scriptable Install System)**
   - Download from: https://nsis.sourceforge.io/Download
   - Install to default location (e.g., `C:\Program Files (x86)\NSIS`)
   - Verify: `makensis /version` in PowerShell

2. **CMake & MSVC**
   - Required to build the C++ application.

3. **Qt 6**
   - Required for building the application.

4. **Git & GitHub**
   - Repo: https://github.com/vincentwetzel/LzyDownloader
   - Must have access to create Releases

## Build Process

### Step 1: Update Extractor Lists

**IMPORTANT:** Before building a new release, you must refresh the extractor lists to ensure the application can handle the latest website changes.

Run the following Python scripts from the project root:
```powershell
python ./update_yt-dlp_extractors.py
python ./update_gallery-dl_extractors.py
```
This will update `extractors_yt-dlp.json` and `extractors_gallery-dl.json`.

### Step 3: Update Version Number

Update the version in `CMakeLists.txt` (or a dedicated version header file).

### Step 4: Build with CMake

Configure and build the project in Release mode.
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

This generates `LzyDownloader.exe` in the build directory.

### Step 5: Deploy Qt Dependencies

Use `windeployqt` to copy necessary Qt DLLs to the build directory.
```powershell
windeployqt build/Release/LzyDownloader.exe
```

### Step 6: Create NSIS Installer

Build the NSIS script to create a Windows installer. You will need to adapt the `LzyDownloader.nsi` script to point to the C++ build output directory instead of the Python `dist/` folder.

```powershell
& 'C:\Program Files (x86)\NSIS\makensis.exe' LzyDownloader.nsi
```

Output:
```
LzyDownloader-Setup-X.X.X.exe
```

## Release to GitHub

### Step 1: Create a Git Tag

```powershell
git tag -a vX.X.X -m "Release version X.X.X"
git push origin vX.X.X
```

### Step 2: Create GitHub Release

Navigate to https://github.com/vincentwetzel/LzyDownloader/releases and:

1. Click "Create a new release"
2. **Tag version:** `vX.X.X` (must match Git tag)
3. **Release title:** `LzyDownloader X.X.X`
4. **Description:** Add release notes.
5. **Attach Assets:** Upload `LzyDownloader-Setup-X.X.X.exe`
6. Click "Publish release"

## Release Checklist

- [ ] Extractor lists updated (`extractors_yt-dlp.json`, `extractors_gallery-dl.json`)
- [ ] Version number updated in `CMakeLists.txt`
- [ ] Built in Release mode with `windeployqt`
- [ ] NSIS installer tested (silent install preserves `settings.ini` and `download_archive.db`)
- [ ] AppData logging verified (logs at `%APPDATA%\LzyDownloader\LzyDownloader.log`)
- [ ] Log rotation verified (old logs cycle automatically, max 5 files at 2 MB each)
- [ ] GitHub release published with installer asset

## Application Data Locations (Windows)

The application stores user data in standard Windows directories:

| File | Location |
|------|----------|
| Settings | `%APPDATA%\LzyDownloader\LzyDownloader\settings.ini` |
| Archive | `%APPDATA%\LzyDownloader\LzyDownloader\download_archive.db` |
| Logs | `%APPDATA%\LzyDownloader\LzyDownloader.log` (with rotation: `.log.1`, `.log.2`, etc.) |

**Important:** The NSIS installer must NOT overwrite `settings.ini`, `download_archive.db`, or log files. These are stored in user data directories, not the installation directory.
