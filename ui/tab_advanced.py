import logging
import os
import sys
import subprocess
import threading

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QComboBox, QCheckBox, QFileDialog, QMessageBox
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal

log = logging.getLogger(__name__)


class AdvancedSettingsTab(QWidget):
    """Advanced settings tab, including folders, sponsorblock, and restore defaults."""

    # Define signals for background thread communication
    update_finished = pyqtSignal(bool, str)
    version_fetched = pyqtSignal(str)

    def __init__(self, main_window):
        super().__init__()
        self.main = main_window
        self.config = main_window.config_manager
        
        # Connect signals to slots
        self.update_finished.connect(self._on_update_finished)
        self.version_fetched.connect(self._on_version_fetched)
        
        self._build_tab_advanced()

    def _build_tab_advanced(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(8, 8, 8, 8)

        # Output folder row
        out_row = QHBoxLayout()
        out_lbl = QLabel("Output folder:")
        # Use ConfigManager.get(section, option, fallback=...)
        out_path = self.config.get("Paths", "completed_downloads_directory", fallback="")
        self.out_display = QLabel(out_path)
        self.out_display.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        btn_out = QPushButton("📁")
        btn_out.setFixedWidth(40)
        btn_out.clicked.connect(self.browse_out)
        out_row.addWidget(out_lbl)
        out_row.addWidget(self.out_display, stretch=1)
        out_row.addWidget(btn_out)
        layout.addLayout(out_row)

        # Temporary folder row
        temp_row = QHBoxLayout()
        temp_lbl = QLabel("Temporary folder:")
        temp_path = self.config.get("Paths", "temporary_downloads_directory", fallback="")
        self.temp_display = QLabel(temp_path)
        self.temp_display.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        btn_temp = QPushButton("📁")
        btn_temp.setFixedWidth(40)
        btn_temp.clicked.connect(self.browse_temp)
        temp_row.addWidget(temp_lbl)
        temp_row.addWidget(self.temp_display, stretch=1)
        temp_row.addWidget(btn_temp)
        layout.addLayout(temp_row)

        # Browser cookies dropdown
        cookies_row = QHBoxLayout()
        cookies_lbl = QLabel("Cookies from browser:")
        self.cookies_combo = QComboBox()
        self.cookies_combo.addItem("None")
        for b in ("chrome", "edge", "firefox", "brave", "vivaldi", "safari"):
            self.cookies_combo.addItem(b)
        cur_browser = self.config.get("General", "cookies_from_browser", fallback="None")
        idx = self.cookies_combo.findText(cur_browser)
        if idx >= 0:
            self.cookies_combo.setCurrentIndex(idx)
        self.cookies_combo.currentTextChanged.connect(self.on_cookies_browser_changed)
        cookies_row.addWidget(cookies_lbl)
        cookies_row.addWidget(self.cookies_combo, stretch=1)
        layout.addLayout(cookies_row)

        # SponsorBlock and Restrict filenames
        self.sponsorblock_cb = QCheckBox("Enable SponsorBlock")
        sponsor_val = self.config.get("General", "sponsorblock", fallback="True")
        self.sponsorblock_cb.setChecked(str(sponsor_val) == "True")
        self.sponsorblock_cb.stateChanged.connect(
            lambda s: self._save_general("sponsorblock", str(bool(s)))
        )

        self.restrict_cb = QCheckBox("Restrict filenames")
        restrict_val = self.config.get("General", "restrict_filenames", fallback="False")
        self.restrict_cb.setChecked(str(restrict_val) == "True")
        self.restrict_cb.stateChanged.connect(
            lambda s: self._save_general("restrict_filenames", str(bool(s)))
        )

        layout.addWidget(self.sponsorblock_cb)
        layout.addWidget(self.restrict_cb)

        # --- yt-dlp Update Section ---
        update_group = QHBoxLayout()
        
        # Dropdown for version channel (stable vs nightly)
        self.update_channel_combo = QComboBox()
        self.update_channel_combo.addItem("Stable (default)", "stable")
        self.update_channel_combo.addItem("Nightly", "nightly")
        # Load saved preference or default to stable
        saved_channel = self.config.get("General", "yt_dlp_update_channel", fallback="stable")
        idx = self.update_channel_combo.findData(saved_channel)
        if idx >= 0:
            self.update_channel_combo.setCurrentIndex(idx)
        self.update_channel_combo.currentIndexChanged.connect(self._on_update_channel_changed)

        self.update_btn = QPushButton("Update yt-dlp")
        self.update_btn.clicked.connect(self._update_yt_dlp)
        
        # Version label
        self.version_lbl = QLabel("Current version: Unknown")
        # Refresh version label after a short delay to ensure yt-dlp path is resolved
        QTimer.singleShot(1000, self._refresh_version_label)

        update_group.addWidget(QLabel("Update Channel:"))
        update_group.addWidget(self.update_channel_combo)
        update_group.addWidget(self.update_btn)
        update_group.addWidget(self.version_lbl)
        update_group.addStretch()
        
        layout.addLayout(update_group)
        # -----------------------------

        # Restore Defaults button
        restore_btn = QPushButton("Restore Defaults")
        restore_btn.clicked.connect(self._restore_defaults)
        layout.addWidget(restore_btn)

        layout.addStretch()
        self.setLayout(layout)

    # --- Event handlers ---
    def browse_out(self):
        """Prompt user to choose new output directory."""
        folder = QFileDialog.getExistingDirectory(self, "Select Output Folder")
        if folder:
            # Normalize path to use system separators (e.g. backslashes on Windows)
            folder = os.path.normpath(folder)
            
            self.config.set("Paths", "completed_downloads_directory", folder)
            self.out_display.setText(folder)
            log.debug(f"Updated output directory: {folder}")
            
            # Automatically set temp directory if it's not set or if we want to enforce a structure
            # Logic: If the user sets an output folder, we can conveniently set the temp folder
            # to be a subdirectory of it.
            
            new_temp = os.path.join(folder, "temp_downloads")
            new_temp = os.path.normpath(new_temp) # Ensure consistency
            
            self.config.set("Paths", "temporary_downloads_directory", new_temp)
            self.temp_display.setText(new_temp)
            log.debug(f"Automatically updated temporary directory to: {new_temp}")
            
            # Ensure the directory exists
            try:
                os.makedirs(new_temp, exist_ok=True)
            except Exception as e:
                log.warning(f"Could not create auto-temp directory: {e}")

    def browse_temp(self):
        """Prompt user to choose new temp directory."""
        folder = QFileDialog.getExistingDirectory(self, "Select Temporary Folder")
        if folder:
            # Normalize path to use system separators
            folder = os.path.normpath(folder)

            self.config.set("Paths", "temporary_downloads_directory", folder)
            self.temp_display.setText(folder)
            log.debug(f"Updated temporary directory: {folder}")

    def on_cookies_browser_changed(self, val):
        """Handle cookies-from-browser dropdown changes."""
        from utils.cookies import is_browser_installed
        if val == "None":
            self._save_general("cookies_from_browser", "None")
            return
        if not is_browser_installed(val):
            QMessageBox.warning(
                self, "Browser Not Found",
                f"The selected browser '{val}' was not detected on this system."
            )
            self.cookies_combo.setCurrentIndex(0)
        else:
            self._save_general("cookies_from_browser", val)

    def _save_general(self, key, val):
        """Save a key to the General config section."""
        self.config.set("General", key, val)

    def _on_update_channel_changed(self, index):
        channel = self.update_channel_combo.itemData(index)
        self._save_general("yt_dlp_update_channel", channel)

    def _refresh_version_label(self):
        """Fetch and display the current yt-dlp version."""
        from core.yt_dlp_worker import get_yt_dlp_version
        
        def fetch():
            ver = get_yt_dlp_version()
            self.version_fetched.emit(str(ver) if ver else "Unknown")

        # Run in background to avoid UI freeze if disk is slow
        t = threading.Thread(target=fetch, daemon=True)
        t.start()

    def _on_version_fetched(self, ver):
        # Check if version string indicates nightly
        is_nightly = "nightly" in ver.lower() or ".dev" in ver.lower()
        channel_text = " (Nightly)" if is_nightly else " (Stable)"
        if ver == "Unknown":
            channel_text = ""
            
        self.version_lbl.setText(f"Current version: {ver}{channel_text}")

    def _update_yt_dlp(self):
        """Run yt-dlp -U (or --update-to nightly) in a background thread."""
        import core.yt_dlp_worker
        import shutil
        
        # Ensure we have the path. If _YT_DLP_PATH is None, try to find it.
        # This handles cases where the worker hasn't run yet.
        target_exe = core.yt_dlp_worker._YT_DLP_PATH
        if not target_exe:
             # Force a check to populate _YT_DLP_PATH if possible
             core.yt_dlp_worker.check_yt_dlp_available()
             target_exe = core.yt_dlp_worker._YT_DLP_PATH

        # Fallback to system path if still not found
        if not target_exe:
            target_exe = shutil.which("yt-dlp")
        
        if not target_exe:
            QMessageBox.critical(self, "Update Failed", "Could not locate yt-dlp executable to update.")
            return

        channel = self.update_channel_combo.currentData()
        
        # Build command
        # If channel is 'nightly', use --update-to nightly
        # If channel is 'stable', use -U (which updates to latest stable)
        cmd = [target_exe]
        if channel == "nightly":
            cmd.extend(["--update-to", "nightly"])
        else:
            cmd.append("-U")

        self.update_btn.setEnabled(False)
        self.update_btn.setText("Updating...")
        
        def run_update():
            log.info(f"Starting yt-dlp update with command: {cmd}")
            try:
                # On Windows, if the exe is in a protected directory (like Program Files), this might fail
                # without admin privileges. We can't easily elevate from here without external tools,
                # so we just try and report the result.
                # CREATE_NO_WINDOW flag for Windows to avoid popping up a console
                creationflags = 0
                if sys.platform == "win32":
                    creationflags = subprocess.CREATE_NO_WINDOW
                
                # IMPORTANT: Add stdin=subprocess.DEVNULL to prevent hanging if the process waits for input
                # Added timeout to prevent infinite hanging
                proc = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    creationflags=creationflags,
                    stdin=subprocess.DEVNULL,
                    timeout=120
                )
                
                success = proc.returncode == 0
                output = proc.stdout + "\n" + proc.stderr
                log.info(f"Update finished. Success: {success}. Output len: {len(output)}")
                
                self.update_finished.emit(success, output)
            except subprocess.TimeoutExpired:
                log.error("Update timed out")
                self.update_finished.emit(False, "Update timed out after 120 seconds.")
            except Exception as e:
                log.exception("Update failed with exception")
                self.update_finished.emit(False, str(e))

        t = threading.Thread(target=run_update, daemon=True)
        t.start()

    def _on_update_finished(self, success, message):
        self.update_btn.setEnabled(True)
        self.update_btn.setText("Update yt-dlp")
        
        # Determine the channel we just updated to/checked against
        channel = self.update_channel_combo.currentData()
        channel_name = "Nightly" if channel == "nightly" else "Stable"
        
        if success:
            # Check the output to see if it was actually updated or already up to date
            if "is up to date" in message:
                msg_title = "Already Up to Date"
                msg_body = f"yt-dlp ({channel_name}) is already at the latest version.\n\nOutput:\n{message}"
                icon = QMessageBox.Icon.Information
            else:
                msg_title = "Update Successful"
                msg_body = f"yt-dlp has been updated to the latest {channel_name} version.\n\nOutput:\n{message}"
                icon = QMessageBox.Icon.Information
            
            msg = QMessageBox(self)
            msg.setWindowTitle(msg_title)
            msg.setText(msg_body)
            msg.setIcon(icon)
            msg.exec()
            
            self._refresh_version_label()
        else:
            QMessageBox.warning(self, "Update Failed", f"Update command failed.\n\nOutput:\n{message}\n\nNote: If yt-dlp is installed in a protected directory, you may need to run this app as Administrator.")

    def _restore_defaults(self):
        """Restore all settings to factory defaults."""
        confirm = QMessageBox.question(
            self, "Confirm Reset",
            "Are you sure you want to restore all settings to defaults?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if confirm == QMessageBox.StandardButton.Yes:
            # Recreate config with defaults
            import os
            if os.path.exists(self.config.ini_path):
                os.remove(self.config.ini_path)
            self.config.load_config()
            
            # Update UI elements
            self.out_display.setText(self.config.get("Paths", "completed_downloads_directory", fallback=""))
            self.temp_display.setText(self.config.get("Paths", "temporary_downloads_directory", fallback=""))
            
            sponsor_val = self.config.get("General", "sponsorblock", fallback="True")
            self.sponsorblock_cb.setChecked(str(sponsor_val) == "True")
            
            restrict_val = self.config.get("General", "restrict_filenames", fallback="False")
            self.restrict_cb.setChecked(str(restrict_val) == "True")
            
            self.cookies_combo.setCurrentIndex(0) # Reset to None
            
            # Reset update channel
            idx = self.update_channel_combo.findData("stable")
            if idx >= 0:
                self.update_channel_combo.setCurrentIndex(idx)

            QMessageBox.information(self, "Restored", "Defaults restored.")

    def open_downloads_folder(self):
        p = self.out_display.text()
        if not p or not os.path.isdir(p):
            QMessageBox.warning(self, "Open folder", f"Folder does not exist: {p}")
            return
        if sys.platform == "win32":
            os.startfile(os.path.normpath(p))
        elif sys.platform == "darwin":
            os.system(f'open "{p}"')
        else:
            os.system(f'xdg-open "{p}"')