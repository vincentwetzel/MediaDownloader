import logging
import os
import sys

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QComboBox, QCheckBox, QFileDialog, QMessageBox
)
from PyQt6.QtCore import Qt

log = logging.getLogger(__name__)


class AdvancedSettingsTab(QWidget):
    """Advanced settings tab, including folders, sponsorblock, and restore defaults."""

    def __init__(self, main_window):
        super().__init__()
        self.main = main_window
        self.config = main_window.config_manager
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
            self.config["Paths"]["completed_downloads_directory"] = folder
            self.out_display.setText(folder)
            self.main.config_manager.save()
            log.debug(f"Updated output directory: {folder}")

    def browse_temp(self):
        """Prompt user to choose new temp directory."""
        folder = QFileDialog.getExistingDirectory(self, "Select Temporary Folder")
        if folder:
            self.config["Paths"]["temporary_downloads_directory"] = folder
            self.temp_display.setText(folder)
            self.main.config_manager.save()
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
        self.config["General"][key] = val
        self.main.config_manager.save()

    def _restore_defaults(self):
        """Restore all settings to factory defaults."""
        confirm = QMessageBox.question(
            self, "Confirm Reset",
            "Are you sure you want to restore all settings to defaults?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if confirm == QMessageBox.StandardButton.Yes:
            self.main.config_manager.restore_defaults()
            QMessageBox.information(self, "Restored", "Defaults restored. Please restart the app.")

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