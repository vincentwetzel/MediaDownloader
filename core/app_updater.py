import requests
import logging
import sys
import os
import subprocess
from core.version import __version__, GITHUB_REPO

logger = logging.getLogger(__name__)

class AppUpdater:
    """
    Handles checking for application updates via GitHub Releases.
    """

    def __init__(self):
        self.current_version = __version__
        self.repo = GITHUB_REPO
        self.api_url = f"https://api.github.com/repos/{self.repo}/releases/latest"

    def check_for_updates(self):
        """
        Queries GitHub for the latest release.
        
        Returns:
            dict or None: A dictionary containing 'version', 'url', and 'body' 
                          if a newer version is found. Returns None if up to date 
                          or if an error occurs.
        """
        if "YourUsername" in self.repo:
            logger.warning("GitHub repository not configured in core/version.py. Skipping update check.")
            return None

        try:
            logger.info(f"Checking for updates from {self.api_url}...")
            response = requests.get(self.api_url, timeout=5)
            response.raise_for_status()
            
            data = response.json()
            latest_tag = data.get("tag_name", "").lstrip("v")
            
            if self._is_newer(latest_tag):
                logger.info(f"New version found: {latest_tag} (Current: {self.current_version})")
                return {
                    "version": latest_tag,
                    "url": data.get("html_url"),
                    "assets": data.get("assets", []),
                    "body": data.get("body", "")
                }
            else:
                logger.info("Application is up to date.")
                return None

        except requests.RequestException as e:
            logger.error(f"Failed to check for updates: {e}")
            return None

    def download_update(self, url, target_path, progress_callback=None):
        """
        Downloads the update file from the given URL to the target path.
        
        Args:
            url (str): The URL of the asset to download.
            target_path (str): The local path to save the file.
            progress_callback (callable, optional): A function that accepts an integer (0-100).
        
        Returns:
            bool: True if successful, False otherwise.
        """
        try:
            logger.info(f"Downloading update from {url} to {target_path}")
            response = requests.get(url, stream=True, timeout=30)
            response.raise_for_status()
            
            total_size = int(response.headers.get('content-length', 0))
            downloaded = 0
            
            with open(target_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    if chunk:
                        f.write(chunk)
                        downloaded += len(chunk)
                        if progress_callback and total_size > 0:
                            progress_callback(int(downloaded * 100 / total_size))
            
            logger.info("Update download completed.")
            return True
        except Exception as e:
            logger.error(f"Failed to download update: {e}")
            return False

    def apply_update(self, downloaded_file):
        """
        Applies the update using the downloaded file.
        If the file is an installer, runs it.
        If it's a binary replacement, schedules the replacement and restart.
        """
        if not getattr(sys, 'frozen', False):
            logger.warning("Cannot perform self-update when running from source.")
            return False

        abs_path = os.path.abspath(downloaded_file)
        if not os.path.exists(abs_path):
            logger.error(f"Update file not found: {abs_path}")
            return False

        # Simple heuristic: if it has 'setup' or 'installer' in name, run it
        filename = os.path.basename(abs_path).lower()
        if "setup" in filename or "installer" in filename or filename.endswith(".msi"):
            logger.info(f"Launching installer: {abs_path}")
            try:
                subprocess.Popen([abs_path], shell=True)
                sys.exit(0)
            except Exception as e:
                logger.error(f"Failed to launch installer: {e}")
                return False
        
        # Otherwise assume it's the main executable replacement
        current_exe = sys.executable
        logger.info(f"Preparing to replace {current_exe} with {abs_path}")
        
        # Create a batch script to handle the swap
        # We use a temporary batch file
        batch_script = f"""
@echo off
timeout /t 2 /nobreak > NUL
move /y "{current_exe}" "{current_exe}.old"
move /y "{abs_path}" "{current_exe}"
start "" "{current_exe}"
del "%~f0"
"""
        batch_path = os.path.join(os.path.dirname(current_exe), "update_restart.bat")
        try:
            with open(batch_path, "w") as f:
                f.write(batch_script)
            
            logger.info(f"Launching update script: {batch_path}")
            # Use Popen to launch independent process
            subprocess.Popen([batch_path], shell=True)
            sys.exit(0)
        except Exception as e:
            logger.error(f"Failed to create/run update script: {e}")
            return False

    def _is_newer(self, latest_version_str):
        """
        Compares the latest version string against the current version.
        Assumes semantic versioning (X.Y.Z).
        """
        try:
            current_parts = [int(x) for x in self.current_version.split('.')]
            latest_parts = [int(x) for x in latest_version_str.split('.')]
            
            # Compare tuples
            return latest_parts > current_parts
        except ValueError:
            logger.warning(f"Version parsing failed. Current: {self.current_version}, Latest: {latest_version_str}")
            return False
