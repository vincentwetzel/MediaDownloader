import logging
import os
import sys
import subprocess
import threading
import time

from core.version import __version__ as APP_VERSION
from core.update_manager import get_gallery_dl_version, download_gallery_dl_update, get_latest_release, _compare_versions
from core.archive_manager import ArchiveManager

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QComboBox, QCheckBox, QFileDialog, QMessageBox, QLineEdit,
    QApplication, QGroupBox, QScrollArea
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal

from ui.tab_advanced_ui import build_media_group, SUBTITLE_LANGUAGES

log = logging.getLogger(__name__)


class AdvancedSettingsTab(QWidget):
    """Advanced settings tab, including folders, sponsorblock, and restore defaults."""

    # Define signals for background thread communication
    update_finished = pyqtSignal(bool, str)
    version_fetched = pyqtSignal(str)
    gallery_dl_update_finished = pyqtSignal(str, str)
    gallery_dl_version_fetched = pyqtSignal(str)
    app_update_finished = pyqtSignal(str, object)

    def __init__(self, main_window, initial_yt_dlp_version: str = "Unknown"):
        super().__init__()
        self.main = main_window
        self.config = main_window.config_manager
        self.archive_manager = ArchiveManager(self.config, self)

        # Connect signals to slots
        self.update_finished.connect(self._on_update_finished)
        self.version_fetched.connect(self._on_version_fetched)
        self.gallery_dl_update_finished.connect(self._on_gallery_dl_update_finished)
        self.gallery_dl_version_fetched.connect(self._on_gallery_dl_version_fetched)
        self.app_update_finished.connect(self._on_app_update_finished)

        self._build_tab_advanced()

        # Set initial version label text
        self.version_lbl.setText(f"Current version: {initial_yt_dlp_version}")
        # Manually trigger the version fetched handler to format the label correctly
        self._on_version_fetched(initial_yt_dlp_version)

    def _get_installed_browsers(self):
        """Return a list of installed browsers, with a preferred order."""
        from utils.cookies import is_browser_installed

        # Browsers to check, in a non-specific order initially
        browsers_to_check = ["edge", "brave", "vivaldi", "safari", "opera", "whale", "chromium"]

        installed_browsers = [b for b in browsers_to_check if is_browser_installed(b)]

        # Prioritize chrome and firefox by inserting them at the front if found
        if is_browser_installed("chrome"):
            installed_browsers.insert(0, "chrome")
        if is_browser_installed("firefox"):
            installed_browsers.insert(0, "firefox")

        return installed_browsers

    def _build_tab_advanced(self):
        # Main layout for the tab, containing the scroll area
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)

        # Scroll Area
        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        main_layout.addWidget(scroll_area)

        # Container widget for all settings
        scroll_content = QWidget()
        scroll_area.setWidget(scroll_content)

        # Layout for the container widget that holds all the settings groups
        layout = QVBoxLayout(scroll_content)
        layout.setContentsMargins(8, 8, 8, 8)

        def add_group(title: str):
            group = QGroupBox(title)
            group_layout = QVBoxLayout()
            group.setLayout(group_layout)
            layout.addWidget(group)
            return group_layout

        config_group = add_group("Configuration")

        # Output folder row
        out_row = QHBoxLayout()
        out_lbl = QLabel("Output folder:")
        out_lbl.setToolTip("Where completed downloads will be saved.")
        out_path = self.config.get("Paths", "completed_downloads_directory", fallback="")
        self.out_display = QLabel(out_path)
        self.out_display.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        btn_out = QPushButton("\U0001F4C1")
        btn_out.setFixedWidth(40)
        btn_out.clicked.connect(self.browse_out)
        btn_out.setToolTip("Browse and select the output folder for completed downloads.")
        out_row.addWidget(out_lbl)
        out_row.addWidget(self.out_display, stretch=1)
        out_row.addWidget(btn_out)
        config_group.addLayout(out_row)

        # Temporary folder row
        temp_row = QHBoxLayout()
        temp_lbl = QLabel("Temporary folder:")
        temp_lbl.setToolTip("Where downloads are stored while in progress before moving to output folder.")
        temp_path = self.config.get("Paths", "temporary_downloads_directory", fallback="")
        self.temp_display = QLabel(temp_path)
        self.temp_display.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        btn_temp = QPushButton("\U0001F4C1")
        btn_temp.setFixedWidth(40)
        btn_temp.clicked.connect(self.browse_temp)
        btn_temp.setToolTip("Browse and select the temporary folder for in-progress downloads.")
        temp_row.addWidget(temp_lbl)
        temp_row.addWidget(self.temp_display, stretch=1)
        temp_row.addWidget(btn_temp)
        config_group.addLayout(temp_row)

        # Theme dropdown
        theme_row = QHBoxLayout()
        theme_lbl = QLabel("Theme:")
        theme_lbl.setToolTip("Choose the application theme.")
        self.theme_combo = QComboBox()
        self.theme_combo.setToolTip("Select the application theme.")
        self.theme_combo.addItem("System", "auto")
        self.theme_combo.addItem("Light", "light")
        self.theme_combo.addItem("Dark", "dark")

        saved_theme = self.config.get("General", "theme", fallback="auto")
        idx = self.theme_combo.findData(saved_theme)
        if idx >= 0:
            self.theme_combo.setCurrentIndex(idx)
        else:
            if saved_theme == "system":
                idx = self.theme_combo.findData("auto")
                if idx >= 0:
                    self.theme_combo.setCurrentIndex(idx)

        self.theme_combo.currentIndexChanged.connect(self._on_theme_changed)

        theme_row.addWidget(theme_lbl)
        theme_row.addWidget(self.theme_combo, stretch=1)
        config_group.addLayout(theme_row)

        auth_group = add_group("Authentication & Access")

        # Browser cookies dropdown (yt-dlp)
        cookies_row = QHBoxLayout()
        cookies_lbl = QLabel("Cookies from browser (Video/Audio):")
        cookies_lbl.setToolTip("Use cookies from your browser to authenticate with websites like YouTube.")
        self.cookies_combo = QComboBox()
        self.cookies_combo.setToolTip(
            "Select a browser to extract cookies from. Required for accessing age-restricted content.")

        installed_browsers = self._get_installed_browsers()
        self.cookies_combo.addItem("None")
        self.cookies_combo.addItems(installed_browsers)

        cur_browser = self.config.get("General", "cookies_from_browser", fallback="None")

        if cur_browser in installed_browsers:
            idx = self.cookies_combo.findText(cur_browser)
        elif installed_browsers:
            first_browser = installed_browsers[0]
            idx = self.cookies_combo.findText(first_browser)
            self._save_general("cookies_from_browser", first_browser)
        else:
            idx = 0
            self._save_general("cookies_from_browser", "None")

        if idx >= 0:
            self.cookies_combo.setCurrentIndex(idx)

        self.cookies_combo.currentTextChanged.connect(self.on_cookies_browser_changed)
        cookies_row.addWidget(cookies_lbl)
        cookies_row.addWidget(self.cookies_combo, stretch=1)
        auth_group.addLayout(cookies_row)

        # Browser cookies dropdown (gallery-dl)
        gallery_cookies_row = QHBoxLayout()
        gallery_cookies_lbl = QLabel("Cookies from browser (Galleries):")
        gallery_cookies_lbl.setToolTip("Use cookies from your browser to authenticate with image hosting sites.")
        self.gallery_cookies_combo = QComboBox()
        self.gallery_cookies_combo.setToolTip(
            "Select a browser to extract cookies from for gallery-dl.")

        self.gallery_cookies_combo.addItem("None")
        self.gallery_cookies_combo.addItems(installed_browsers)

        cur_gallery_browser = self.config.get("General", "gallery_cookies_from_browser", fallback="None")

        if cur_gallery_browser in installed_browsers:
            idx = self.gallery_cookies_combo.findText(cur_gallery_browser)
        else:
            idx = 0
            self._save_general("gallery_cookies_from_browser", "None")

        if idx >= 0:
            self.gallery_cookies_combo.setCurrentIndex(idx)

        self.gallery_cookies_combo.currentTextChanged.connect(self.on_gallery_cookies_browser_changed)
        gallery_cookies_row.addWidget(gallery_cookies_lbl)
        gallery_cookies_row.addWidget(self.gallery_cookies_combo, stretch=1)
        auth_group.addLayout(gallery_cookies_row)

        # JavaScript Runtime Path
        js_runtime_row = QHBoxLayout()
        js_runtime_lbl = QLabel("JS Runtime (Deno/Node.js):")
        js_runtime_lbl.setToolTip("Path to a JavaScript runtime for handling anti-bot challenges on some websites.")
        js_runtime_path = self.config.get("General", "js_runtime_path", fallback="")
        self.js_runtime_display = QLabel(js_runtime_path)
        self.js_runtime_display.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        btn_js_runtime = QPushButton("\U0001F4C1")
        btn_js_runtime.setFixedWidth(40)
        btn_js_runtime.clicked.connect(self.browse_js_runtime)
        btn_js_runtime.setToolTip("Browse and select a JavaScript runtime executable (e.g., deno.exe or node.exe).")
        js_runtime_row.addWidget(js_runtime_lbl)
        js_runtime_row.addWidget(self.js_runtime_display, stretch=1)
        js_runtime_row.addWidget(btn_js_runtime)
        auth_group.addLayout(js_runtime_row)

        template_group = add_group("Output Template")

        # Output Filename Pattern
        pattern_row = QHBoxLayout()
        pattern_lbl = QLabel("Filename Pattern:")
        pattern_lbl.setToolTip("Define the output filename pattern using yt-dlp template variables.")

        default_template = "%(title)s [%(uploader)s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s"
        current_template = self.config.get("General", "output_template", fallback=default_template)

        self.pattern_input = QLineEdit(current_template)
        self.pattern_input.setToolTip("Enter yt-dlp output template. Click Save to apply.")

        btn_save_pattern = QPushButton("Save")
        btn_save_pattern.setFixedWidth(60)
        btn_save_pattern.setToolTip("Validate and save the filename pattern.")
        btn_save_pattern.clicked.connect(self._save_pattern)

        btn_reset_pattern = QPushButton("Reset")
        btn_reset_pattern.setFixedWidth(60)
        btn_reset_pattern.setToolTip("Reset to default pattern.")
        btn_reset_pattern.clicked.connect(self._reset_pattern)

        pattern_row.addWidget(pattern_lbl)
        pattern_row.addWidget(self.pattern_input, stretch=1)
        pattern_row.addWidget(btn_save_pattern)
        pattern_row.addWidget(btn_reset_pattern)
        template_group.addLayout(pattern_row)

        options_group = add_group("Download Options")

        # External Downloader dropdown
        downloader_row = QHBoxLayout()
        downloader_lbl = QLabel("External Downloader:")
        downloader_lbl.setToolTip("Use an external downloader like aria2 for faster downloads.")
        self.downloader_combo = QComboBox()
        self.downloader_combo.setToolTip("Select an external downloader to use with yt-dlp. We recommend aria2.")
        self.downloader_combo.addItem("None", "none")
        self.downloader_combo.addItem("aria2", "aria2")

        saved_downloader = self.config.get("General", "external_downloader", fallback="none")
        idx = self.downloader_combo.findData(saved_downloader)
        if idx >= 0:
            self.downloader_combo.setCurrentIndex(idx)

        self.downloader_combo.currentIndexChanged.connect(self._on_downloader_changed)

        downloader_row.addWidget(downloader_lbl)
        downloader_row.addWidget(self.downloader_combo, stretch=1)
        options_group.addLayout(downloader_row)

        # SponsorBlock and Restrict filenames
        self.sponsorblock_cb = QCheckBox("Enable SponsorBlock")
        self.sponsorblock_cb.setToolTip("Automatically skip sponsored segments, intros, and outros in videos.")
        sponsor_val = self.config.get("General", "sponsorblock", fallback="True")
        self.sponsorblock_cb.setChecked(str(sponsor_val) == "True")
        self.sponsorblock_cb.stateChanged.connect(
            lambda s: self._save_general("sponsorblock", str(bool(s)))
        )
        options_group.addWidget(self.sponsorblock_cb)

        self.restrict_cb = QCheckBox("Restrict filenames")
        self.restrict_cb.setToolTip(
            "Use only ASCII characters in filenames (safer for older systems but may shorten names).")
        restrict_val = self.config.get("General", "restrict_filenames", fallback="False")
        self.restrict_cb.setChecked(str(restrict_val) == "True")
        self.restrict_cb.stateChanged.connect(
            lambda s: self._save_general("restrict_filenames", str(bool(s)))
        )
        options_group.addWidget(self.restrict_cb)

        layout.addWidget(build_media_group(self))

        # Download Archive Group
        archive_group = add_group("Download Archive")
        
        archive_row = QHBoxLayout()
        self.archive_cb = QCheckBox("Use Download Archive")
        self.archive_cb.setToolTip("Keep a record of downloaded files to prevent re-downloading them.")
        archive_val = self.config.get("General", "download_archive", fallback="False")
        self.archive_cb.setChecked(str(archive_val) == "True")
        self.archive_cb.stateChanged.connect(
            lambda s: self._save_general("download_archive", str(bool(s)))
        )
        
        self.view_archive_btn = QPushButton("View Archive")
        self.view_archive_btn.setToolTip("View the contents of the download archive file.")
        self.view_archive_btn.clicked.connect(self.archive_manager.view_archive)
        
        self.clear_archive_btn = QPushButton("Clear Archive")
        self.clear_archive_btn.setToolTip("Clear the download archive file.")
        self.clear_archive_btn.clicked.connect(self.archive_manager.clear_archive)
        
        archive_row.addWidget(self.archive_cb)
        archive_row.addStretch()
        archive_row.addWidget(self.view_archive_btn)
        archive_row.addWidget(self.clear_archive_btn)
        
        archive_group.addLayout(archive_row)

        updates_group = add_group("Updates")

        # --- yt-dlp Update Section ---
        update_group = QHBoxLayout()

        self.update_channel_combo = QComboBox()
        self.update_channel_combo.addItem("Nightly (recommended)", "nightly")
        self.update_channel_combo.addItem("Stable", "stable")
        self.update_channel_combo.setToolTip(
            "Choose between nightly (recommended for latest fixes) or stable builds of yt-dlp.")
        saved_channel = self.config.get("General", "yt_dlp_update_channel", fallback="nightly")
        idx = self.update_channel_combo.findData(saved_channel)
        if idx >= 0:
            self.update_channel_combo.setCurrentIndex(idx)
        self.update_channel_combo.currentIndexChanged.connect(self._on_update_channel_changed)

        self.update_btn = QPushButton("Update yt-dlp")
        self.update_btn.setToolTip("Check for and install the latest version of yt-dlp.")
        self.update_btn.clicked.connect(self._update_yt_dlp)

        self.version_lbl = QLabel("Current version: Unknown")

        update_group.addWidget(QLabel("Update Channel:"))
        update_group.addWidget(self.update_channel_combo)
        update_group.addWidget(self.update_btn)
        update_group.addWidget(self.version_lbl)
        update_group.addStretch()
        updates_group.addLayout(update_group)

        # --- gallery-dl Update Section ---
        gallery_update_group = QHBoxLayout()
        self.gallery_update_btn = QPushButton("Update gallery-dl")
        self.gallery_update_btn.setToolTip("Check for and install the latest version of gallery-dl.")
        self.gallery_update_btn.clicked.connect(self._update_gallery_dl)
        
        self.gallery_version_lbl = QLabel("Current version: Unknown")
        
        gallery_update_group.addWidget(self.gallery_update_btn)
        gallery_update_group.addWidget(self.gallery_version_lbl)
        gallery_update_group.addStretch()
        updates_group.addLayout(gallery_update_group)

        # Application update controls
        app_update_group = QHBoxLayout()
        self.app_version_lbl = QLabel(f"App version: {APP_VERSION}")
        self.check_app_update_btn = QPushButton("Check for App Update")
        self.check_app_update_btn.setToolTip("Check GitHub for a newer version of MediaDownloader.")
        self.check_app_update_btn.clicked.connect(self._check_app_update)

        self.auto_check_cb = QCheckBox("Check for app updates on startup")
        auto_val = self.config.get("General", "auto_check_updates", fallback="True")
        self.auto_check_cb.setChecked(str(auto_val) == "True")
        self.auto_check_cb.stateChanged.connect(lambda s: self._save_general("auto_check_updates", str(bool(s))))

        app_update_group.addWidget(self.app_version_lbl)
        app_update_group.addWidget(self.check_app_update_btn)
        app_update_group.addWidget(self.auto_check_cb)
        app_update_group.addStretch()
        updates_group.addLayout(app_update_group)

        maintenance_group = add_group("Maintenance")

        # Restore Defaults button
        self.restore_btn = QPushButton("Restore Defaults")
        self.restore_btn.setToolTip("Reset all download settings to their default values.")
        self.restore_btn.clicked.connect(self._restore_defaults)
        maintenance_group.addWidget(self.restore_btn)

        layout.addStretch()

        # Initialize gallery-dl version
        self._fetch_gallery_dl_version()

    # --- Event handlers ---
    def browse_out(self):
        """Prompt user to choose new output directory."""
        folder = QFileDialog.getExistingDirectory(self, "Select Output Folder")
        if folder:
            folder = os.path.normpath(folder)
            self.config.set("Paths", "completed_downloads_directory", folder)
            self.out_display.setText(folder)
            log.debug(f"Updated output directory: {folder}")

            # Automatically set temp directory if it's not set or if we want to enforce a structure
            # Logic: If the user sets an output folder, we can conveniently set the temp folder
            # to be a subdirectory of it.

            new_temp = os.path.join(folder, "temp_downloads")
            new_temp = os.path.normpath(new_temp)  # Ensure consistency

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
            folder = os.path.normpath(folder)
            self.config.set("Paths", "temporary_downloads_directory", folder)
            self.temp_display.setText(folder)
            log.debug(f"Updated temporary directory: {folder}")

    def browse_js_runtime(self):
        """Prompt user to choose the JavaScript runtime executable (e.g., deno.exe or node.exe)."""
        filter_str = "Executable Files (*.exe);;All Files (*)" if sys.platform == "win32" else "All Files (*)"
        file_path, _ = QFileDialog.getOpenFileName(self, "Select JavaScript Runtime Executable", "", filter_str)
        if file_path:
            file_path = os.path.normpath(file_path)
            self.config.set("General", "js_runtime_path", file_path)
            self.js_runtime_display.setText(file_path)
            log.debug(f"Updated JavaScript runtime path: {file_path}")

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

    def on_gallery_cookies_browser_changed(self, val):
        """Handle cookies-from-browser dropdown changes for gallery-dl."""
        from utils.cookies import is_browser_installed
        if val == "None":
            self._save_general("gallery_cookies_from_browser", "None")
            return
        if not is_browser_installed(val):
            QMessageBox.warning(
                self, "Browser Not Found",
                f"The selected browser '{val}' was not detected on this system."
            )
            self.gallery_cookies_combo.setCurrentIndex(0)
        else:
            self._save_general("gallery_cookies_from_browser", val)

    def _save_general(self, key, val):
        """Save a key to the General config section."""
        self.config.set("General", key, val)

    def _get_system_theme(self):
        """
        Detects if the system is in dark mode.
        Returns 'dark' or 'light'. Defaults to 'dark' on error or non-Windows.
        """
        try:
            if sys.platform == "win32":
                import winreg
                key_path = r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize"
                with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key_path) as key:
                    value, _ = winreg.QueryValueEx(key, "AppsUseLightTheme")
                    return "light" if value == 1 else "dark"
        except Exception:
            pass
        return "dark"

    def _on_theme_changed(self, index):
        theme = self.theme_combo.itemData(index)
        self._save_general("theme", theme)

        try:
            import qdarktheme
            if hasattr(qdarktheme, 'setup_theme'):
                qdarktheme.setup_theme(theme)
            elif hasattr(qdarktheme, 'load_stylesheet'):
                app = QApplication.instance()
                if app:
                    if theme == 'auto':
                        theme_to_load = self._get_system_theme()
                    else:
                        theme_to_load = theme
                    app.setStyleSheet(qdarktheme.load_stylesheet(theme_to_load))
            else:
                log.error("qdarktheme module found but has no setup_theme or load_stylesheet")
        except ImportError:
            log.warning("qdarktheme not found. Theme change will not be applied immediately.")
        except Exception as e:
            log.error(f"Failed to apply theme '{theme}': {e}")

    def _on_downloader_changed(self, index):
        downloader = self.downloader_combo.itemData(index)
        self._save_general("external_downloader", downloader)

    def _on_update_channel_changed(self, index):
        channel = self.update_channel_combo.currentData(index)
        self._save_general("yt_dlp_update_channel", channel)

    def _on_subs_format_changed(self, index):
        fmt = self.subs_format_combo.itemData(index)
        self._save_general("subtitles_format", fmt)

    def _check_app_update(self):
        """Check GitHub for a newer app release and prompt the user to update."""
        self.check_app_update_btn.setEnabled(False)
        self.check_app_update_btn.setText("Checking...")

        owner = 'vincentwetzel'
        repo = 'MediaDownloader'

        def bg():
            try:
                import core.update_manager as updater
                rel = updater.get_latest_release(owner, repo)
                if not rel:
                    self.app_update_finished.emit("error", "Could not fetch release information from GitHub.")
                    return
                tag = rel.get('tag_name') or rel.get('name') or ''
                cmp = updater._compare_versions(APP_VERSION, tag)
                if cmp == -1:
                    self.app_update_finished.emit("update_available", rel)
                else:
                    self.app_update_finished.emit("up_to_date", None)
            except Exception as e:
                log.exception('App update check failed')
                self.app_update_finished.emit("error", str(e))

        t = threading.Thread(target=bg, daemon=True)
        t.start()

    def _on_app_update_finished(self, status, data):
        self.check_app_update_btn.setEnabled(True)
        self.check_app_update_btn.setText("Check for App Update")

        if status == "error":
            QMessageBox.warning(self, 'Update Check Failed', str(data))
        elif status == "up_to_date":
            QMessageBox.information(self, 'Up To Date', f'No update available. Current version: {APP_VERSION}')
        elif status == "update_available":
            self._prompt_update(data)

    def _prompt_update(self, release_info: dict):
        tag = release_info.get('tag_name') or release_info.get('name') or 'unknown'
        body = release_info.get('body') or ''
        short_body = (body[:2000] + '...') if len(body) > 2000 else body
        text = f"<b>A new version is available: {tag}</b><br><br>{short_body.replace('\\n', '<br>')}"

        msg = QMessageBox(self)
        msg.setWindowTitle('Update Available')
        msg.setTextFormat(Qt.TextFormat.RichText)
        msg.setText(text)

        update_btn = msg.addButton('Update and Restart', QMessageBox.ButtonRole.YesRole)
        later_btn = msg.addButton('Later', QMessageBox.ButtonRole.NoRole)
        msg.exec()

        if msg.clickedButton() == update_btn:
            # Show a "downloading" message
            self.main.show_status_message("Downloading update...", 5000)

            def bg_download_and_apply():
                try:
                    import core.update_manager as updater
                    temp_dir = self.config.get("Paths", "temporary_downloads_directory", fallback="")
                    ok, path = updater.check_and_download_update(
                        'vincentwetzel',
                        'MediaDownloader',
                        target_dir=temp_dir or None
                    )
                    if not ok or not path:
                        QTimer.singleShot(0, lambda: QMessageBox.warning(self, 'Update Failed',
                                                                         'Failed to download the update. Please try again later.'))
                        return

                    # This will trigger the update and the application will exit.
                    updater.perform_self_update(path)

                except Exception as e:
                    log.exception('Failed to apply update')
                    QTimer.singleShot(0, lambda: QMessageBox.warning(self, 'Update Error',
                                                                     f"An unexpected error occurred during the update process: {e}"))

            t = threading.Thread(target=bg_download_and_apply, daemon=True)
            t.start()

    def _on_version_fetched(self, ver):
        # A nightly build can be identified by 'nightly', '.dev', or a version string with more than two dots (e.g., YYYY.MM.DD.HHMMSS)
        is_nightly = "nightly" in ver.lower() or ".dev" in ver.lower() or ver.count('.') > 2
        channel_text = " (Nightly)" if is_nightly else " (Stable)"
        if ver == "Unknown":
            channel_text = ""
        self.version_lbl.setText(f"Current version: {ver}{channel_text}")

    def _update_yt_dlp(self):
        """
        Checks for a yt-dlp update against GitHub, and if available, runs the
        update process. This avoids running the updater unnecessarily.
        """
        self.update_btn.setEnabled(False)
        self.update_btn.setText("Checking...")

        # Run the check in a background thread to keep the UI responsive
        thread = threading.Thread(target=self._run_yt_dlp_update_check, daemon=True)
        thread.start()

    def _run_yt_dlp_update_check(self):
        """
        Performs the logic for checking and potentially running the yt-dlp update.
        This should be run in a background thread.
        """
        import core.yt_dlp_worker

        channel = self.update_channel_combo.currentData()
        repo_owner = "yt-dlp"
        repo_name = "yt-dlp-nightly-builds" if channel == "nightly" else "yt-dlp"

        try:
            # 1. Get local version
            local_version = core.yt_dlp_worker.get_yt_dlp_version(force_check=True)
            if not local_version:
                self.update_finished.emit(False, "Could not determine local yt-dlp version.")
                return

            # 2. Get remote version
            release_info = get_latest_release(repo_owner, repo_name)
            if not release_info:
                self.update_finished.emit(False, "Could not fetch latest release info from GitHub.")
                return
            
            remote_version = release_info.get('tag_name') or release_info.get('name') or ''
            if not remote_version:
                self.update_finished.emit(False, "Could not determine remote version from GitHub release.")
                return

            # 3. Compare versions
            if _compare_versions(local_version, remote_version) != -1:
                # Local is greater than or equal to remote
                self.update_finished.emit(True, "yt-dlp is already up to date.")
                return

            # 4. If we get here, an update is needed. Proceed with the update process.
            self.update_btn.setText("Updating...")
            
            target_exe = core.yt_dlp_worker._YT_DLP_PATH
            if not target_exe:
                self.update_finished.emit(False, "Could not locate yt-dlp executable to update.")
                return

            update_args = ["--update-to", channel] if channel == "nightly" else ["-U"]
            cmd_list = [target_exe] + update_args

            # On Windows, launch detached and start monitor.
            if sys.platform == "win32":
                creation_flags = subprocess.CREATE_NO_WINDOW
                subprocess.Popen(
                    cmd_list, 
                    creationflags=creation_flags, 
                    stdout=subprocess.DEVNULL, 
                    stderr=subprocess.DEVNULL, 
                    close_fds=True
                )
                monitor_thread = threading.Thread(target=self._monitor_update_completion, args=(local_version,), daemon=True)
                monitor_thread.start()
            else:
                # On other platforms, run synchronously and get result.
                proc = subprocess.run(
                    cmd_list,
                    capture_output=True,
                    text=True,
                    stdin=subprocess.DEVNULL,
                    timeout=120
                )
                success = proc.returncode == 0
                output = proc.stdout + "\n" + proc.stderr
                self.update_finished.emit(success, output)

        except Exception as e:
            log.exception("yt-dlp update check failed with an exception.")
            self.update_finished.emit(False, f"An error occurred: {e}")

    def _monitor_update_completion(self, original_version):
        """
        Polls in the background to see if the yt-dlp version has changed.
        This is used on Windows where the update process is detached.
        """
        import core.yt_dlp_worker
        start_time = time.time()
        timeout = 120  # 2 minutes

        while time.time() - start_time < timeout:
            time.sleep(2)  # Check every 2 seconds
            
            new_version = core.yt_dlp_worker.get_yt_dlp_version(force_check=True)
            
            if new_version and new_version != original_version:
                log.info(f"yt-dlp update detected: {original_version} -> {new_version}")
                self.update_finished.emit(True, f"Successfully updated to version {new_version}")
                return

        # If the loop finishes, it timed out
        log.warning("yt-dlp update check timed out.")
        self.update_finished.emit(False, "Update check timed out after 2 minutes. Please check your connection or try again.")

    def _on_update_finished(self, success, message):
        self.update_btn.setEnabled(True)
        self.update_btn.setText("Update yt-dlp")

        if success:
            if "is up to date" in message:
                msg_title = "Already Up to Date"
                msg_body = "yt-dlp is already at the latest version."
                QMessageBox.information(self, msg_title, msg_body)
                # No further action needed, label is already correct.
            else:
                msg_title = "Update Successful"
                msg_body = message
                QMessageBox.information(self, msg_title, msg_body)
                self._refresh_version_label()
        else:
            QMessageBox.warning(self, "Update Failed", message)

    def _on_subs_lang_changed(self, index):
        lang_code = self.subs_lang_combo.itemData(index)
        self._save_general("subtitles_langs", lang_code)

    def _restore_defaults(self):
        """Restore all settings to factory defaults."""
        confirm = QMessageBox.question(
            self, "Confirm Reset",
            "Are you sure you want to restore all settings to defaults?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if confirm == QMessageBox.StandardButton.Yes:
            import os
            if os.path.exists(self.config.ini_path):
                os.remove(self.config.ini_path)
            self.config.load_config()

            self.out_display.setText(self.config.get("Paths", "completed_downloads_directory", fallback=""))
            self.temp_display.setText(self.config.get("Paths", "temporary_downloads_directory", fallback=""))

            sponsor_val = self.config.get("General", "sponsorblock", fallback="True")
            self.sponsorblock_cb.setChecked(str(sponsor_val) == "True")

            restrict_val = self.config.get("General", "restrict_filenames", fallback="False")
            self.restrict_cb.setChecked(str(restrict_val) == "True")

            # Reset subtitle options
            embed_subs_val = self.config.get("General", "subtitles_embed", fallback="False")
            self.embed_subs_cb.setChecked(str(embed_subs_val) == "True")

            write_subs_val = self.config.get("General", "subtitles_write", fallback="False")
            self.write_subs_cb.setChecked(str(write_subs_val) == "True")

            write_auto_subs_val = self.config.get("General", "subtitles_write_auto", fallback="False")
            self.write_auto_subs_cb.setChecked(str(write_auto_subs_val) == "True")

            embed_chapters_val = self.config.get("General", "embed_chapters", fallback="True")
            self.embed_chapters_cb.setChecked(str(embed_chapters_val) == "True")

            saved_lang = self.config.get("General", "subtitles_langs", fallback="en")
            idx = self.subs_lang_combo.findData(saved_lang)
            if idx >= 0:
                self.subs_lang_combo.setCurrentIndex(idx)
            else:
                idx_en = self.subs_lang_combo.findData("en")
                if idx_en >= 0:
                    self.subs_lang_combo.setCurrentIndex(idx_en)

            # Reset subtitle format to SRT
            idx = self.subs_format_combo.findData("srt")
            if idx >= 0:
                self.subs_format_combo.setCurrentIndex(idx)
            self._save_general("subtitles_format", "srt")

            self.cookies_combo.clear()
            installed_browsers = self._get_installed_browsers()
            self.cookies_combo.addItem("None")
            self.cookies_combo.addItems(installed_browsers)

            if installed_browsers:
                first_browser = installed_browsers[0]
                idx = self.cookies_combo.findText(first_browser)
                self._save_general("cookies_from_browser", first_browser)
            else:
                idx = 0
                self._save_general("cookies_from_browser", "None")

            if idx >= 0:
                self.cookies_combo.setCurrentIndex(idx)

            # Reset gallery-dl cookies
            self.gallery_cookies_combo.clear()
            self.gallery_cookies_combo.addItem("None")
            self.gallery_cookies_combo.addItems(installed_browsers)
            self.gallery_cookies_combo.setCurrentIndex(0)
            self._save_general("gallery_cookies_from_browser", "None")

            idx = self.update_channel_combo.findData("nightly")
            if idx >= 0:
                self.update_channel_combo.setCurrentIndex(idx)

            idx = self.theme_combo.findData("auto")
            if idx >= 0:
                self.theme_combo.setCurrentIndex(idx)

            idx = self.downloader_combo.findData("none")
            if idx >= 0:
                self.downloader_combo.setCurrentIndex(idx)

            self.js_runtime_display.setText(self.config.get("General", "js_runtime_path", fallback=""))

            default_template = "%(title)s [%(uploader)s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s"
            self.pattern_input.setText(default_template)
            
            # Reset download archive
            self.archive_cb.setChecked(False)
            self._save_general("download_archive", "False")

            start_tab = getattr(self.main, "tab_start", None)
            if start_tab:
                start_tab.reset_to_defaults()

            QMessageBox.information(self, "Restored", "Defaults restored.")

    def _save_pattern(self):
        new_pattern = self.pattern_input.text().strip()
        if not new_pattern:
            QMessageBox.warning(self, "Invalid Pattern", "Pattern cannot be empty.")
            return

        sender = self.sender()
        original_text = sender.text() if sender else "Save"
        if sender:
            sender.setText("Checking...")
            sender.setEnabled(False)
        QApplication.processEvents()

        try:
            if not self._validate_output_template(new_pattern):
                msg = QMessageBox(self)
                msg.setIcon(QMessageBox.Icon.Warning)
                msg.setWindowTitle("Invalid Pattern")
                msg.setText("The entered output template appears to be invalid.")
                msg.setInformativeText(
                    'Please check the <a href="https://github.com/yt-dlp/yt-dlp?tab=readme-ov-file#output-template">yt-dlp documentation</a> for proper syntax.')
                msg.setTextFormat(Qt.TextFormat.RichText)
                msg.exec()
                return

            self._save_general("output_template", new_pattern)
            QMessageBox.information(self, "Saved", "Output filename pattern saved.")
        finally:
            if sender:
                sender.setText(original_text)
                sender.setEnabled(True)

    def _reset_pattern(self):
        default_template = "%(title)s [%(uploader)s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s"
        self.pattern_input.setText(default_template)
        self._save_general("output_template", default_template)
        QMessageBox.information(self, "Reset", "Output filename pattern reset to default.")

    def _validate_output_template(self, template):
        """Final, robust validation for yt-dlp output template based on stderr."""
        if not template or not template.strip():
            return False

        if template.count('(') != template.count(')'):
            log.warning("Template validation failed: unbalanced parentheses.")
            return False

        import shutil
        import core.yt_dlp_worker

        target_exe = core.yt_dlp_worker._YT_DLP_PATH
        if not target_exe:
            core.yt_dlp_worker.check_yt_dlp_available()
            target_exe = core.yt_dlp_worker._YT_DLP_PATH

        if not target_exe:
            target_exe = shutil.which("yt-dlp")

        if not target_exe:
            log.warning("Could not find yt-dlp to validate template. Skipping advanced validation.")
            return True

        test_url = "https://www.youtube.com/shorts/dvX6HdyzbHM"

        cmd = [
            target_exe,
            "--get-filename",
            "--output-na-placeholder", "MISSING_KEY_ERROR",
            "-o",
            template,
            test_url
        ]

        creation_flags = 0
        if sys.platform == "win32" and getattr(sys, "frozen", False):
            creation_flags = subprocess.CREATE_NO_WINDOW

        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                encoding='utf-8',
            )

            if "MISSING_KEY_ERROR" in proc.stdout:
                return False

            return True

        except subprocess.TimeoutExpired:
            log.error("Template validation process timed out. Assuming template is valid to avoid blocking user.")
            return True
        except Exception as e:
            log.error(f"Template validation process itself failed: {e}. Assuming template is valid.")
            return True

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

    def _fetch_gallery_dl_version(self):
        """Fetch gallery-dl version in background."""
        self.gallery_version_lbl.setText("Current version: (checking...)")
        def bg():
            version = get_gallery_dl_version()
            self.gallery_dl_version_fetched.emit(version or "Not Found")
        
        t = threading.Thread(target=bg, daemon=True)
        t.start()

    def _on_gallery_dl_version_fetched(self, version):
        self.gallery_version_lbl.setText(f"Current version: {version}")

    def _update_gallery_dl(self):
        """Downloads the latest gallery-dl release in a background thread."""
        self.gallery_update_btn.setEnabled(False)
        self.gallery_update_btn.setText("Updating...")

        def run_update():
            try:
                status, message = download_gallery_dl_update()
                self.gallery_dl_update_finished.emit(status, message)
            except Exception as e:
                log.exception("gallery-dl update failed with exception")
                self.gallery_dl_update_finished.emit("failed", str(e))

        t = threading.Thread(target=run_update, daemon=True)
        t.start()

    def _on_gallery_dl_update_finished(self, status, message):
        self.gallery_update_btn.setEnabled(True)
        self.gallery_update_btn.setText("Update gallery-dl")
        
        if status == "success":
            QMessageBox.information(self, "Update Successful", message)
            self._fetch_gallery_dl_version() # Refresh version label
        elif status == "up_to_date":
            QMessageBox.information(self, "Up to Date", message)
        else:
            QMessageBox.warning(self, "Update Failed", message)
    
    def _refresh_version_label(self):
        """Refreshes the yt-dlp version label by re-running the version check."""
        import core.yt_dlp_worker
        
        self.version_lbl.setText("Current version: (checking...)")
        def do_refresh():
            # This forces a re-check of the yt-dlp version
            new_version = core.yt_dlp_worker.get_yt_dlp_version(force_check=True)
            if new_version:
                self.version_fetched.emit(new_version)

        # Run the refresh in a background thread to avoid blocking the UI
        t = threading.Thread(target=do_refresh, daemon=True)
        t.start()
