"""Simple GitHub-based updater utilities.

This module provides functions to check the latest GitHub release for a
repository, compare it to the local app version, download a release asset
and perform a basic self-update on Windows (for frozen/compiled executables).

Design notes:
- Uses unauthenticated GitHub API (rate-limited). If you need higher rate
  limits, pass an auth token via the `GITHUB_TOKEN` env var (not implemented
  here but easily added).
- Asset selection: you can pass a desired asset filename. If omitted, the
  first `.exe` asset in the release will be chosen for Windows.
- Self-update strategy (Windows): downloads new exe to a temp file, writes a
  small batch script that waits for the running process to exit, moves the
  new exe into place, restarts the app, and deletes the script.

This is intentionally lightweight: test locally before using in production.
"""

from __future__ import annotations

import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Optional, Tuple

try:
    import requests
except Exception:
    requests = None

from core.version import __version__ as LOCAL_VERSION

log = logging.getLogger(__name__)


def _normalize_version(v: str) -> Tuple[int, ...]:
    """Normalize a version string into a tuple of integers for comparison.

    This is a lightweight approach that extracts numeric components. It
    handles simple semantic versions like "1.2.3" and also tags like
    "v1.2.3". It is not a full semver parser but is sufficient for basic
    version checks.
    """
    if not v:
        return (0,)
    nums = re.findall(r"(\d+)", str(v))
    return tuple(int(x) for x in nums) if nums else (0,)


def _compare_versions(local: str, remote: str) -> int:
    """Compare two version strings.

    Returns: -1 if remote > local, 0 if equal, 1 if local > remote
    """
    a = _normalize_version(local)
    b = _normalize_version(remote)
    # lexicographic compare padded by zeros
    la, lb = len(a), len(b)
    n = max(la, lb)
    a = a + (0,) * (n - la)
    b = b + (0,) * (n - lb)
    if a == b:
        return 0
    return -1 if a < b else 1


def get_latest_release(owner: str, repo: str) -> Optional[dict]:
    """Return the latest release JSON from GitHub or None on failure."""
    if requests is None:
        raise RuntimeError("requests is required for updater but is not installed")
    api = f"https://api.github.com/repos/{owner}/{repo}/releases/latest"
    try:
        r = requests.get(api, timeout=15)
        if r.status_code == 200:
            return r.json()
        log.warning("GitHub API returned %s for %s/%s", r.status_code, owner, repo)
    except Exception as e:
        log.exception("Failed to fetch latest release: %s", e)
    return None


def find_asset_for_platform(release_info: dict, asset_name: Optional[str] = None) -> Optional[dict]:
    """Select an asset dict from a release.

    If `asset_name` is provided, try to match it exactly. Otherwise, choose
    the official installer first, then any portable .exe, then the first asset.
    """
    assets = release_info.get("assets", []) if release_info else []
    if not assets:
        return None
    if asset_name:
        for a in assets:
            if a.get("name") == asset_name:
                return a
    
    # Prioritize the official installer
    for a in assets:
        n = a.get("name", "").lower()
        if "installer" in n and n.endswith(".exe"):
            return a

    # Then, look for a portable executable
    for a in assets:
        n = a.get("name", "").lower()
        if "portable" in n and n.endswith(".exe"):
            return a

    # Fallback to the first asset if no other options are found
    return assets[0]


def check_for_update(owner: str, repo: str) -> Tuple[bool, Optional[str], Optional[dict]]:
    """Check GitHub for a newer release.

    Returns a tuple: (is_newer_available, latest_version_tag, asset_dict_or_None)
    """
    rel = get_latest_release(owner, repo)
    if not rel:
        return False, None, None
    tag = rel.get('tag_name') or rel.get('name') or ''
    cmp_result = _compare_versions(LOCAL_VERSION, tag)
    is_newer = cmp_result == -1
    asset = find_asset_for_platform(rel, None)
    return is_newer, tag, asset


def download_asset(asset: dict, target_path: str) -> bool:
    """Download a release asset to target_path. Returns True on success."""
    if requests is None:
        raise RuntimeError("requests is required for updater but is not installed")
    url = asset.get('browser_download_url')
    if not url:
        log.error("Asset missing download url")
        return False
    log.info("Downloading asset from %s to %s", url, target_path)
    try:
        with requests.get(url, stream=True, timeout=60) as r:
            r.raise_for_status()
            with open(target_path, 'wb') as fh:
                for chunk in r.iter_content(chunk_size=8192):
                    if chunk:
                        fh.write(chunk)
        return True
    except Exception:
        log.exception("Failed to download asset")
        return False


def _write_windows_update_script(tmp_dir: str, new_path: str, target_exe: str) -> str:
    """Write a batch script that waits for the running exe to exit, then
    replaces it with `new_path` and restarts it. Returns path to the script.
    """
    script_path = os.path.join(tmp_dir, 'updater_apply.bat')
    # Use a more robust loop that waits for the main process to exit
    script = f"""@echo off
echo Waiting for application to exit...
setlocal enabledelayedexpansion
set NEW_EXE="{new_path}"
set TARGET_EXE="{target_exe}"
set TIMEOUT=30
:loop
tasklist /FI "IMAGENAME eq %TARGET_EXE%" | findstr /I "%TARGET_EXE%" >nul
if %ERRORLEVEL%==0 (
    timeout /t 1 >nul
    set /a TIMEOUT-=1
    if !TIMEOUT! equ 0 (
        echo Timeout waiting for application to exit.
        exit /b 1
    )
    goto loop
)
echo Replacing executable...
move /Y %NEW_EXE% %TARGET_EXE%
if %ERRORLEVEL% neq 0 (
    echo Failed to replace executable.
    exit /b 1
)
echo Starting updated app...
start "" %TARGET_EXE%
del "%~f0"
"""
    try:
        with open(script_path, 'w', encoding='utf-8') as fh:
            fh.write(script)
        return script_path
    except Exception:
        log.exception("Failed to write update script")
        return ""


def _is_nsis_installer(exe_path: str) -> bool:
    """Check if an executable is an NSIS installer by filename heuristic."""
    return 'setup' in os.path.basename(exe_path).lower()


def perform_self_update(downloaded_exe_path: str) -> bool:
    """Perform a Windows self-update.

    For NSIS installers: runs the installer in silent mode with /S /D parameter.
    For raw executables: uses a batch script to replace and restart.
    
    Only supported on frozen executables (PyInstaller) on Windows.
    """
    try:
        if not getattr(sys, 'frozen', False):
            log.error("Self-update is supported only for frozen executables")
            return False
        
        # Determine if this is an NSIS installer or a raw exe
        is_nsis = _is_nsis_installer(downloaded_exe_path)
        
        if is_nsis:
            # Run the NSIS installer with silent flags
            # /S = silent install
            # /D = specify install directory (must be the app installation directory)
            app_dir = os.path.dirname(sys.executable)
            cmd = [downloaded_exe_path, '/S', f'/D={app_dir}']
            log.info("Running NSIS installer: %s", cmd)
            # Launch installer detached so app can exit
            creation_flags = subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
            subprocess.Popen(cmd, creationflags=creation_flags)
            log.info("Launched NSIS installer, exiting application to allow update")
            os._exit(0)
        else:
            # For raw executables, use the batch script approach
            target_exe = sys.executable
            tmp_dir = os.path.dirname(downloaded_exe_path) or tempfile.gettempdir()
            script = _write_windows_update_script(tmp_dir, downloaded_exe_path, target_exe)
            if script:
                # Launch the updater script detached
                subprocess.Popen(['cmd', '/c', script], creationflags=0)
                log.info("Launched updater script %s, exiting application to allow update", script)
                # Exit so script can replace the running exe
                os._exit(0)
            else:
                log.error("Failed to create update script")
                return False
    except Exception:
        log.exception("Self-update failed")
        return False


def check_and_download_update(owner: str, repo: str, preferred_asset_name: Optional[str] = None) -> Tuple[bool, Optional[str]]:
    """Check GitHub and download the update if available.

    Returns (downloaded_boolean, path_to_downloaded_file_or_None)
    """
    is_newer, tag, asset = check_for_update(owner, repo)
    if not is_newer or not asset:
        return False, None
    tmp = tempfile.mkdtemp(prefix='md_update_')
    name = asset.get('name') or preferred_asset_name or 'update_asset'
    target = os.path.join(tmp, name)
    ok = download_asset(asset, target)
    return (ok, target if ok else None)


if __name__ == '__main__':
    # Simple local test runner for developers. Not used by the application UI.
    import argparse
    
    # Add the project's root directory to the sys.path
    sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from core.version import __version__ as LOCAL_VERSION

    p = argparse.ArgumentParser()
    p.add_argument('--owner', default='vincentwetzel')
    p.add_argument('--repo', default='MediaDownloader')
    p.add_argument('--test-update', action='store_true')
    args = p.parse_args()

    if args.test_update:
        # Create a dummy executable
        dummy_exe = os.path.join(tempfile.gettempdir(), 'dummy.exe')
        with open(dummy_exe, 'w') as f:
            f.write('dummy executable')
        
        # Create a fake update
        fake_update = os.path.join(tempfile.gettempdir(), 'fake_update.exe')
        with open(fake_update, 'w') as f:
            f.write('fake update')
            
        # Set the executable path to the dummy executable
        sys.executable = dummy_exe
        
        # Set the frozen attribute to True to simulate a frozen application
        setattr(sys, 'frozen', True)
        
        # Call the perform_self_update function
        perform_self_update(fake_update)
    else:
        print('Local version:', LOCAL_VERSION)
        newer, tag, asset = check_for_update(args.owner, args.repo)
        print('Latest tag:', tag)
        print('Newer available:', newer)
        if asset:
            print('Asset chosen:', asset.get('name'))
