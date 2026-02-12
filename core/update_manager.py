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

from core.version import __version__ as LOCAL_VERSION, GITHUB_REPO
from core.binary_manager import get_binary_path

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


def _resolve_repo(owner: Optional[str], repo: Optional[str]) -> Tuple[str, str]:
    """Resolve owner and repo, defaulting to GITHUB_REPO if not provided."""
    if owner and repo:
        return owner, repo
    if GITHUB_REPO and "/" in GITHUB_REPO:
        parts = GITHUB_REPO.split("/", 1)
        return parts[0], parts[1]
    # Fallback
    return owner or "vincentwetzel", repo or "MediaDownloader"


def get_latest_release(owner: Optional[str] = None, repo: Optional[str] = None) -> Optional[dict]:
    """Return the latest release JSON from GitHub or None on failure."""
    owner, repo = _resolve_repo(owner, repo)
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
    
    is_frozen = getattr(sys, 'frozen', False)

    # If running from source, prioritize the source code zip
    if not is_frozen:
        for a in assets:
            n = a.get("name", "").lower()
            if "source code" in n and n.endswith(".zip"):
                return a

    if asset_name:
        for a in assets:
            if a.get("name") == asset_name:
                return a
    
    # Prioritize the official installer
    for a in assets:
        n = a.get("name", "").lower()
        if ("installer" in n or "setup" in n) and (n.endswith(".exe") or n.endswith(".msi")):
            return a

    # Then, look for a portable executable
    for a in assets:
        n = a.get("name", "").lower()
        if "portable" in n and n.endswith(".exe"):
            return a

    # Fallback to the first asset if no other options are found
    return assets[0] if assets else None


def check_for_update(owner: Optional[str] = None, repo: Optional[str] = None) -> Tuple[bool, Optional[str], Optional[dict]]:
    """Check GitHub for a newer release.

    Returns a tuple: (is_newer_available, latest_version_tag, asset_dict_or_None)
    """
    owner, repo = _resolve_repo(owner, repo)
    rel = get_latest_release(owner, repo)
    if not rel:
        return False, None, None
    tag = rel.get('tag_name') or rel.get('name') or ''
    cmp_result = _compare_versions(LOCAL_VERSION, tag)
    is_newer = cmp_result == -1
    asset = find_asset_for_platform(rel, None)
    return is_newer, tag, asset


def download_asset(asset: dict, target_path: str, progress_callback=None) -> bool:
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
            total_size = int(r.headers.get("content-length", 0))
            downloaded = 0
            with open(target_path, 'wb') as fh:
                for chunk in r.iter_content(chunk_size=8192):
                    if chunk:
                        fh.write(chunk)
                        if progress_callback and total_size > 0:
                            downloaded += len(chunk)
                            progress_callback(int(downloaded * 100 / total_size))
        return True
    except Exception:
        log.exception("Failed to download asset")
        return False


def _get_project_root() -> str:
    """Return the absolute path to the project's root directory."""
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _write_self_destruct_script(runner_dir: str) -> str:
    """Writes a script to delete the update runner directory."""
    script_path = os.path.join(runner_dir, "self_destruct.bat" if sys.platform == "win32" else "self_destruct.sh")
    
    if sys.platform == "win32":
        script_content = f"""
@echo off
echo "Self-destructing updater..."
timeout /t 2 /nobreak > nul
rmdir /s /q "{runner_dir}"
"""
    else:
        script_content = f"""
#!/bin/bash
echo "Self-destructing updater..."
sleep 2
rm -rf "{runner_dir}"
"""
    try:
        with open(script_path, "w", encoding="utf-8") as f:
            f.write(script_content)
        if sys.platform != "win32":
            os.chmod(script_path, 0o755)
        return script_path
    except Exception:
        log.exception("Failed to write self-destruct script")
        return ""

def _is_nsis_installer(exe_path: str) -> bool:
    """Check if an executable is an NSIS installer by filename heuristic."""
    return 'setup' in os.path.basename(exe_path).lower()


def _write_exe_swap_script(current_exe: str, new_exe: str) -> str:
    """Write a batch script that swaps in a new executable and restarts the app."""
    batch_path = os.path.join(os.path.dirname(current_exe), "update_restart.bat")
    script_content = f"""
@echo off
timeout /t 2 /nobreak > NUL
move /y "{current_exe}" "{current_exe}.old"
move /y "{new_exe}" "{current_exe}"
start "" "{current_exe}"
del "%~f0"
"""
    try:
        with open(batch_path, "w", encoding="utf-8") as f:
            f.write(script_content)
        return batch_path
    except Exception:
        log.exception("Failed to write executable swap script")
        return ""


def _write_installer_cleanup_script(installer_path: str, install_dir: str, executable_path: str) -> str:
    """Write a batch script that runs the installer, relaunches the app, and then deletes the installer."""
    batch_path = os.path.join(os.path.dirname(installer_path), "update_installer.bat")
    # NSIS /D parameter must be the last one and must NOT be quoted, even if it contains spaces.
    script_content = f"""
@echo off
timeout /t 2 /nobreak > NUL
start /wait "" "{installer_path}" /S /D={install_dir}
start "" "{executable_path}"
del "{installer_path}"
del "%~f0"
"""
    try:
        with open(batch_path, "w", encoding="utf-8") as f:
            f.write(script_content)
        return batch_path
    except Exception:
        log.exception("Failed to write installer cleanup script")
        return ""


def perform_self_update(downloaded_path: str) -> bool:
    """
    Performs a self-update for both frozen and non-frozen applications.
    
    - For frozen executables (NSIS installers): runs the installer silently.
    - For non-frozen (source) execution: unpacks the release zip and restarts.
    """
    try:
        is_frozen = getattr(sys, 'frozen', False)
        
        if is_frozen and _is_nsis_installer(downloaded_path):
            app_dir = os.path.dirname(sys.executable)
            
            cleanup_script = _write_installer_cleanup_script(downloaded_path, app_dir, sys.executable)
            if cleanup_script:
                log.info("Launching installer cleanup script: %s", cleanup_script)
                subprocess.Popen(["cmd.exe", "/c", cleanup_script], creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
            else:
                # Fallback if script creation fails
                cmd = [downloaded_path, '/S', f'/D={app_dir}']
                log.info("Running NSIS installer (fallback): %s", cmd)
                subprocess.Popen(cmd, creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)

            log.info("Launched NSIS installer, exiting application to allow update.")
            os._exit(0)
            return True
        
        if is_frozen and downloaded_path.lower().endswith(".msi"):
            cmd = ["msiexec", "/i", downloaded_path]
            log.info("Launching MSI installer: %s", cmd)
            subprocess.Popen(cmd, creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
            log.info("Launched MSI installer, exiting application to allow update.")
            os._exit(0)
            return True

        if is_frozen and downloaded_path.lower().endswith(".exe"):
            current_exe = sys.executable
            swap_script = _write_exe_swap_script(current_exe, downloaded_path)
            if not swap_script:
                return False
            log.info("Launching update swap script: %s", swap_script)
            subprocess.Popen(["cmd.exe", "/c", swap_script], creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
            os._exit(0)
            return True

        # Handle zip-based releases for source/portable updates
        runner_temp_dir = tempfile.mkdtemp(prefix="updater_runner_")
        self_destruct_script = _write_self_destruct_script(runner_temp_dir)
        
        # This is the path to the python executable that will run the update runner
        python_executable = sys.executable
        
        # This is the path to the update runner script
        update_runner_script = os.path.join(_get_project_root(), "update_runner.py")
        
        # This is the command that will be executed to run the update
        cmd = [
            python_executable,
            update_runner_script,
            "--pid", str(os.getpid()),
            "--file", downloaded_path,
            "--destination", _get_project_root(),
            "--launch", sys.executable,
            "--self-destruct-script", self_destruct_script,
        ]

        log.info("Launching update runner with command: %s", cmd)
        subprocess.Popen(cmd, creationflags=subprocess.DETACHED_PROCESS)
        log.info("Update runner launched, exiting main application.")
        os._exit(0)

        return True

    except Exception:
        log.exception("Self-update failed")
        return False



def check_and_download_update(
    owner: Optional[str] = None,
    repo: Optional[str] = None,
    preferred_asset_name: Optional[str] = None,
    target_dir: Optional[str] = None,
    progress_callback=None,
) -> Tuple[bool, Optional[str]]:
    """Check GitHub and download the update if available.

    Returns (downloaded_boolean, path_to_downloaded_file_or_None)
    """
    owner, repo = _resolve_repo(owner, repo)
    is_newer, tag, asset = check_for_update(owner, repo)
    if not is_newer or not asset:
        return False, None
    tmp = target_dir or tempfile.mkdtemp(prefix='md_update_')
    if target_dir:
        os.makedirs(tmp, exist_ok=True)
    name = asset.get('name') or preferred_asset_name or 'update_asset'
    target = os.path.join(tmp, name)
    ok = download_asset(asset, target, progress_callback=progress_callback)
    return (ok, target if ok else None)


def get_gallery_dl_version() -> Optional[str]:
    """Gets the version of the bundled gallery-dl binary."""
    binary_path = get_binary_path("gallery-dl")
    if not binary_path:
        return None
    try:
        # gallery-dl --version outputs "gallery-dl <version>"
        result = subprocess.run(
            [binary_path, "--version"],
            capture_output=True,
            text=True,
            check=True,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
        )
        output = result.stdout.strip()
        # Example output: gallery-dl 1.25.0
        match = re.search(r"gallery-dl\s+([\d\.]+)", output)
        if match:
            return match.group(1)
        return output # fallback to full output
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        log.error(f"Failed to get gallery-dl version: {e}")
        return None


def check_for_gallery_dl_update() -> Tuple[bool, Optional[str], Optional[dict]]:
    """Check GitHub for a newer gallery-dl release.

    Returns a tuple: (is_newer_available, latest_version_tag, asset_dict_or_None)
    """
    owner, repo = "mikf", "gallery-dl"
    current_version = get_gallery_dl_version()
    if not current_version:
        # If we can't determine the current version, we can't compare.
        # We could assume an update is available, but it's safer to not update.
        return False, None, None

    rel = get_latest_release(owner, repo)
    if not rel:
        return False, None, None
    tag = rel.get('tag_name') or rel.get('name') or ''
    # gallery-dl tags are just the version number, e.g. "1.25.0"
    cmp_result = _compare_versions(current_version, tag)
    is_newer = cmp_result == -1
    
    # gallery-dl releases have assets like gallery-dl.exe
    asset_name = "gallery-dl.exe" if sys.platform == "win32" else "gallery-dl"
    asset = find_asset_for_platform(rel, asset_name)
    
    return is_newer, tag, asset


def download_gallery_dl_update(progress_callback=None) -> Tuple[str, str]:
    """
    Checks for a gallery-dl update, and if one is available, downloads it
    to the correct binary path.

    Returns a tuple of (status, message).
    status can be: 'success', 'up_to_date', 'no_asset', 'check_failed', 'failed'.
    """
    try:
        is_newer, tag, asset = check_for_gallery_dl_update()
    except Exception as e:
        log.exception("Failed to check for gallery-dl update.")
        return "check_failed", f"Failed to check for updates: {e}"

    current_version = get_gallery_dl_version() or "unknown"

    if not tag:
        log.warning("Could not determine latest gallery-dl version from GitHub.")
        return "check_failed", "Could not determine the latest version from GitHub."

    if not is_newer:
        log.info(f"gallery-dl is up to date (current: {current_version}, latest: {tag}).")
        return "up_to_date", f"gallery-dl is already up to date (version {current_version})."

    if not asset:
        log.warning(f"Found newer gallery-dl version {tag}, but no suitable asset was found for this platform.")
        return "no_asset", f"A new version ({tag}) is available, but a compatible file for your system was not found."

    binary_path = get_binary_path("gallery-dl")
    if not binary_path:
        log.error("Could not determine path for gallery-dl binary.")
        return "failed", "Could not determine the path for the gallery-dl binary."

    # Create a temporary file to download to, then move it into place.
    # This is safer than downloading directly to the final destination.
    temp_dir = tempfile.mkdtemp(prefix="gallery-dl-update-")
    download_path = os.path.join(temp_dir, os.path.basename(binary_path))

    log.info(f"Downloading gallery-dl update {tag} to {download_path}")
    success = download_asset(asset, download_path, progress_callback)

    if success:
        try:
            # Make a backup of the old binary
            backup_path = binary_path + ".bak"
            if os.path.exists(binary_path):
                shutil.move(binary_path, backup_path)
                log.info(f"Backed up old gallery-dl to {backup_path}")
            
            # Move the new binary into place
            shutil.move(download_path, binary_path)
            log.info(f"Moved new gallery-dl to {binary_path}")

            # On non-windows, make sure it's executable
            if sys.platform != "win32":
                os.chmod(binary_path, 0o755)

            # Clean up backup
            if os.path.exists(backup_path):
                os.remove(backup_path)

            return "success", f"gallery-dl successfully updated to version {tag}."
        except Exception as e:
            log.exception(f"Failed to move downloaded gallery-dl into place: {e}")
            # Try to restore backup
            if os.path.exists(backup_path):
                shutil.move(backup_path, binary_path)
            return "failed", f"Failed to move downloaded file: {e}"
        finally:
            shutil.rmtree(temp_dir)
    else:
        log.error("Failed to download gallery-dl update.")
        shutil.rmtree(temp_dir)
        return "failed", "Failed to download the update."


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
