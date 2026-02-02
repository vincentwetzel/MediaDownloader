# MediaDownloader Update & Release Guide

This document describes how to build, package, and release MediaDownloader with auto-update support.

## Prerequisites

1. **NSIS (Nullsoft Scriptable Install System)**
   - Download from: https://nsis.sourceforge.io/Download
   - Install to default location (e.g., `C:\Program Files (x86)\NSIS`)
   - Verify: `makensis /version` in PowerShell

2. **PyInstaller** (already in requirements.txt)
   - Used to compile the app to `dist/MediaDownloader/`

3. **Git & GitHub**
   - Repo: https://github.com/vincentwetzel/MediaDownloader
   - Must have access to create Releases

## Build Process

### Step 1: Download Binary Dependencies

Download yt-dlp and ffmpeg for all platforms using the provided script:

```powershell
.\download_binaries.ps1
```

This populates `/bin/` with:
```
bin/
  ├── windows/
  │   ├── yt-dlp.exe
  │   ├── ffmpeg.exe
  │   └── ffprobe.exe
  ├── linux/
  │   ├── yt-dlp
  │   └── ffmpeg-7.0.2-amd64-static/ (extracted tar.xz)
  └── macos/
      ├── yt-dlp_macos
      ├── ffmpeg
      └── ffprobe
```

**Note:** The `/bin/` directory is git-ignored to keep the repo lean. The script handles downloading from official sources.

### Step 2: Update Version Number

Edit `core/version.py` and bump the version (semantic versioning):
```python
__version__ = "0.1.0"  # Increment from "0.0.1"
```

### Step 3: Build with PyInstaller

From the project root:
```powershell
pyinstaller main.spec
```

This generates:
```
dist/MediaDownloader/
  ├── MediaDownloader.exe
  ├── bin/
  │   ├── windows/
  │   │   ├── yt-dlp.exe
  │   │   ├── ffmpeg.exe
  │   │   └── ffprobe.exe
  │   ├── linux/
  │   └── macos/
  ├── ... (other runtime files)
```

### Step 4: Test Locally (Optional)

Test the installer on your machine:
```powershell
.\MediaDownloader-Setup-0.0.1.exe
```

After installation, verify:
- App launches from Start Menu or Program Files
- Downloads work correctly
- Check Advanced Settings > "Check for App Update" (should say "Up to Date")

### Step 5: Create NSIS Installer

Build the NSIS script to create a Windows installer:
```powershell
& 'C:\Program Files (x86)\NSIS\makensis.exe' MediaDownloader.nsi
```

Output:
```
MediaDownloader-Setup-0.0.1.exe  (or whatever version you set in core/version.py)
```

> **Note:** The NSIS script (`MediaDownloader.nsi`) reads from `dist/MediaDownloader/`, so PyInstaller must complete first.

## Release to GitHub

### Step 1: Create a Git Tag

```powershell
git add core/version.py main.spec MediaDownloader.nsi download_binaries.ps1  # or all changes
git commit -m "Release 0.0.1"
git tag -a v0.0.1 -m "Release version 0.0.1"
git push origin v0.0.1
```

### Step 2: Create GitHub Release

Navigate to https://github.com/vincentwetzel/MediaDownloader/releases and:

1. Click "Create a new release"
2. **Tag version:** `v0.0.1` (must match Git tag)
3. **Release title:** `MediaDownloader 0.0.1`
4. **Description:** Add release notes (changelog), e.g.:
   ```
   ## Features
   - GitHub-based auto-update system
   - Improved file move handling after downloads
   - Enhanced UI tooltips and error messages
   
   ## Bug Fixes
   - Fixed signal handler connection for file moves
   - Improved snapshot fallback detection
   
   ## Installation
   Download `MediaDownloader-Setup-0.0.1.exe` and run it.
   ```
5. **Attach Assets:** Upload `MediaDownloader-Setup-0.0.1.exe`
6. Click "Publish release"

### Step 3: Verify Auto-Update

Once the release is live:

1. Run the app or wait for startup check (~2 seconds)
2. Advanced Settings > "Check for App Update"
3. Should detect the new release and prompt to download/install
4. Click "Yes" to download and apply the update
5. App will restart with the new version

## Update Flow (User Experience)

1. User launches MediaDownloader.exe (from Program Files or Start Menu)
2. App starts, checks GitHub for newer release (2 seconds after startup by default)
3. If newer release found:
   - Dialog shows release tag and changelog
   - User clicks "Yes" to download and install
   - Installer downloads to temp folder
   - App exits and runs the NSIS installer in silent mode (`/S /D=...`)
   - NSIS replaces files and restarts the app
4. App launches with new version

## Auto-Update Configuration

Users can control auto-update behavior in Advanced Settings:

- **Check for App Update** button: Manual check at any time
- **Check for app updates on startup** checkbox: Enable/disable automatic startup checks (default: enabled)
- **App version** label: Displays current version

These settings are persisted to `settings.ini` under `[General]`:
- `auto_check_updates` = True/False

## Troubleshooting

### NSIS Installer Build Fails

- Verify `dist/MediaDownloader/` exists and contains all files
- Verify PyInstaller completed without errors
- Check that `MediaDownloader.nsi` can find the files (paths are relative to project root)

### Update Check Returns "No Release"

- Verify GitHub Release tag matches `v<version>` (e.g., `v0.0.1`)
- Verify installer `.exe` is attached to the Release (not just the source code)
- Check your GitHub repo is public (or auth token is configured)

### Silent NSIS Install Fails

- NSIS `/S /D=<path>` flags require the exact installation directory
- If app is in `C:\Program Files\MediaDownloader\`, the installer must use `/D=C:\Program Files\MediaDownloader`
- Admin privileges may be required to write to `Program Files`

### App Doesn't Restart After Update

- Ensure the NSIS installer successfully replaced files (check temp folder for leftover scripts)
- Verify no files are locked by the old process (use Process Explorer)
- Check logs in `logs/MediaDownloader.log` for update errors

## Architecture Notes

- **Updater module:** `core/updater.py`
  - Queries GitHub API for latest release
  - Downloads installer asset
  - Runs silent NSIS installer with correct flags
  
- **UI hooks:** `ui/tab_advanced.py` and `ui/main_window.py`
  - Manual check button with changelog display
  - Startup check (can be disabled in settings)
  
- **Version file:** `core/version.py`
  - Single source of truth for app version
  - Used by updater to compare against GitHub release tag
  
- **Installer script:** `MediaDownloader.nsi`
  - NSIS configuration for packaging the app
  - Copies all files from `dist/MediaDownloader/` recursively
  - Supports silent install for updates (`/S /D=...`)

## Best Practices

1. **Always increment version in `core/version.py` before building.**
2. **Test the installer locally before publishing.**
3. **Include meaningful release notes (changelog) in GitHub Release.**
4. **Keep NSIS installer script up-to-date if folder structure changes.**
5. **Monitor logs for update failures** and communicate known issues to users.

## Future Enhancements

- GitHub API token support for higher rate limits
- Automatic nightly builds and releases
- Rollback to previous version option
- Delta updates (only changed files)
- Silent automatic updates (no prompt, happens in background)
