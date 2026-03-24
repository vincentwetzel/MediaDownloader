# MediaDownloader C++ Update & Release Guide

This document describes how to build, package, and release the C++ version of MediaDownloader with auto-update support.

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
   - Repo: https://github.com/vincentwetzel/MediaDownloader
   - Must have access to create Releases

## Build Process

### Step 1: Download Binary Dependencies

Ensure `bin/` contains the necessary executables:
```
bin/
  ├── yt-dlp.exe
  ├── ffmpeg.exe
  └── ffprobe.exe
```

### Step 2: Update Version Number

Update the version in `CMakeLists.txt` (or a dedicated version header file).

### Step 3: Build with CMake

Configure and build the project in Release mode.
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

This generates `MediaDownloader.exe` in the build directory.

### Step 4: Deploy Qt Dependencies

Use `windeployqt` to copy necessary Qt DLLs to the build directory.
```powershell
windeployqt build/Release/MediaDownloader.exe
```

### Step 5: Create NSIS Installer

Build the NSIS script to create a Windows installer. You will need to adapt the `MediaDownloader.nsi` script to point to the C++ build output directory instead of the Python `dist/` folder.

```powershell
& 'C:\Program Files (x86)\NSIS\makensis.exe' MediaDownloader.nsi
```

Output:
```
MediaDownloader-Setup-X.X.X.exe
```

## Release to GitHub

### Step 1: Create a Git Tag

```powershell
git tag -a vX.X.X -m "Release version X.X.X"
git push origin vX.X.X
```

### Step 2: Create GitHub Release

Navigate to https://github.com/vincentwetzel/MediaDownloader/releases and:

1. Click "Create a new release"
2. **Tag version:** `vX.X.X` (must match Git tag)
3. **Release title:** `MediaDownloader X.X.X`
4. **Description:** Add release notes.
5. **Attach Assets:** Upload `MediaDownloader-Setup-X.X.X.exe`
6. Click "Publish release"

## Update Flow (User Experience)

The update flow remains identical to the Python version:
1. App checks GitHub for newer release.
2. If found, prompts user.
3. Downloads installer.
4. Runs silent installer to replace files.
5. Restarts app.

**Crucial:** The C++ installer must NOT overwrite `settings.ini` or `download_archive.db`.
