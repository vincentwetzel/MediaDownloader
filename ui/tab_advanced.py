import logging
import os
import sys
import subprocess
import threading
import time
import webbrowser
import core.yt_dlp_worker

from core.version import __version__ as APP_VERSION

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QComboBox, QCheckBox, QFileDialog, QMessageBox, QLineEdit, QApplication, QGroupBox
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal

log = logging.getLogger(__name__)


class AdvancedSettingsTab(QWidget):
    """Advanced settings tab, including folders, sponsorblock, and restore defaults."""

    # Subtitle language: backend code -> display name (GUI shows display name, config stores code)
    SUBTITLE_LANGUAGES = {
        "af": "Afrikaans",
        "am": "Amharic",
        "ar": "Arabic",
        "as": "Assamese",
        "az": "Azerbaijani",
        "be": "Belarusian",
        "bg": "Bulgarian",
        "bn": "Bengali",
        "bs": "Bosnian",
        "ca": "Catalan",
        "cs": "Czech",
        "cy": "Welsh",
        "da": "Danish",
        "de": "German",
        "el": "Greek",
        "en": "English",
        "en-GB": "English (United Kingdom)",
        "en-US": "English (United States)",
        "es": "Spanish",
        "es-419": "Spanish (Latin America)",
        "es-ES": "Spanish (Spain)",
        "et": "Estonian",
        "eu": "Basque",
        "fa": "Persian",
        "fi": "Finnish",
        "fil": "Filipino",
        "fr": "French",
        "fr-CA": "French (Canada)",
        "gl": "Galician",
        "gu": "Gujarati",
        "he": "Hebrew",
        "hi": "Hindi",
        "hr": "Croatian",
        "hu": "Hungarian",
        "hy": "Armenian",
        "id": "Indonesian",
        "is": "Icelandic",
        "it": "Italian",
        "ja": "Japanese",
        "ka": "Georgian",
        "kk": "Kazakh",
        "km": "Khmer",
        "kn": "Kannada",
        "ko": "Korean",
        "ky": "Kyrgyz",
        "lo": "Lao",
        "lt": "Lithuanian",
        "lv": "Latvian",
        "mk": "Macedonian",
        "ml": "Malayalam",
        "mn": "Mongolian",
        "mr": "Marathi",
        "ms": "Malay",
        "my": "Burmese",
        "ne": "Nepali",
        "nl": "Dutch",
        "no": "Norwegian",
        "or": "Odia",
        "pa": "Punjabi",
        "pl": "Polish",
        "pt": "Portuguese",
        "pt-BR": "Portuguese (Brazil)",
        "ro": "Romanian",
        "ru": "Russian",
        "si": "Sinhala",
        "sk": "Slovak",
        "sl": "Slovenian",
        "sq": "Albanian",
        "sr": "Serbian",
        "sv": "Swedish",
        "sw": "Swahili",
        "ta": "Tamil",
        "te": "Telugu",
        "th": "Thai",
        "tr": "Turkish",
        "uk": "Ukrainian",
        "ur": "Urdu",
        "uz": "Uzbek",
        "vi": "Vietnamese",
        "zh-Hans": "Chinese (Simplified)",
        "zh-Hant": "Chinese (Traditional)",
        "zu": "Zulu",
    }

    # Define signals for background thread communication
    update_finished = pyqtSignal(bool, str)
    version_fetched = pyqtSignal(str)

    def __init__(self, main_window, initial_yt_dlp_version: str = "Unknown"):
        super().__init__()
        self.main = main_window
        self.config = main_window.config_manager
        
        # Connect signals to slots
        self.update_finished.connect(self._on_update_finished)
        self.version_fetched.connect(self._on_version_fetched)
        
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
        layout = QVBoxLayout()
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

        # Browser cookies dropdown
        cookies_row = QHBoxLayout()
        cookies_lbl = QLabel("Cookies from browser:")
        cookies_lbl.setToolTip("Use cookies from your browser to authenticate with websites like YouTube.")
        self.cookies_combo = QComboBox()
        self.cookies_combo.setToolTip("Select a browser to extract cookies from. Required for accessing age-restricted content.")

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
        self.restrict_cb.setToolTip("Use only ASCII characters in filenames (safer for older systems but may shorten names).")
        restrict_val = self.config.get("General", "restrict_filenames", fallback="False")
        self.restrict_cb.setChecked(str(restrict_val) == "True")
        self.restrict_cb.stateChanged.connect(
            lambda s: self._save_general("restrict_filenames", str(bool(s)))
        )
        options_group.addWidget(self.restrict_cb)

        media_group = add_group("Media & Subtitles")

        self.embed_subs_cb = QCheckBox("Embed subtitles")
        self.embed_subs_cb.setToolTip("Embed subtitles in the video file.")
        embed_subs_val = self.config.get("General", "subtitles_embed", fallback="False")
        self.embed_subs_cb.setChecked(str(embed_subs_val) == "True")
        self.embed_subs_cb.stateChanged.connect(
            lambda s: self._save_general("subtitles_embed", str(bool(s)))
        )
        media_group.addWidget(self.embed_subs_cb)

        self.write_subs_cb = QCheckBox("Write subtitles (separate file)")
        self.write_subs_cb.setToolTip("Download subtitles to a separate file.")
        write_subs_val = self.config.get("General", "subtitles_write", fallback="False")
        self.write_subs_cb.setChecked(str(write_subs_val) == "True")
        self.write_subs_cb.stateChanged.connect(
            lambda s: self._save_general("subtitles_write", str(bool(s)))
        )
        media_group.addWidget(self.write_subs_cb)

        self.write_auto_subs_cb = QCheckBox("Write automatic subtitles")
        self.write_auto_subs_cb.setToolTip("Download automatic subtitles if available.")
        write_auto_subs_val = self.config.get("General", "subtitles_write_auto", fallback="False")
        self.write_auto_subs_cb.setChecked(str(write_auto_subs_val) == "True")
        self.write_auto_subs_cb.stateChanged.connect(
            lambda s: self._save_general("subtitles_write_auto", str(bool(s)))
        )
        media_group.addWidget(self.write_auto_subs_cb)

        subs_lang_row = QHBoxLayout()
        subs_lang_lbl = QLabel("Subtitle language:")
        subs_lang_lbl.setToolTip("Language for subtitles.")
        self.subs_lang_combo = QComboBox()
        self.subs_lang_combo.setToolTip("Select a language for subtitles.")

        for code, name in self.SUBTITLE_LANGUAGES.items():
            self.subs_lang_combo.addItem(name, code)

        saved_lang = self.config.get("General", "subtitles_langs", fallback="en")
        idx = self.subs_lang_combo.findData(saved_lang)
        if idx >= 0:
            self.subs_lang_combo.setCurrentIndex(idx)
        else:
            # Saved value not in list (e.g. from old text entry); fall back to English
            idx_en = self.subs_lang_combo.findData("en")
            if idx_en >= 0:
                self.subs_lang_combo.setCurrentIndex(idx_en)
                self._save_general("subtitles_langs", "en")

        self.subs_lang_combo.currentIndexChanged.connect(self._on_subs_lang_changed)

        subs_lang_row.addWidget(subs_lang_lbl)
        subs_lang_row.addWidget(self.subs_lang_combo, stretch=1)
        media_group.addLayout(subs_lang_row)

        subs_format_row = QHBoxLayout()
        subs_format_lbl = QLabel("Convert subtitles to format:")
        subs_format_lbl.setToolTip("Format to convert subtitles to. Select 'None' to keep the original format.")
        self.subs_format_combo = QComboBox()
        self.subs_format_combo.setToolTip("Select a subtitle format, or 'None' to keep the original.")
        self.subs_format_combo.addItems(['None', 'srt', 'vtt', 'ass', 'lrc'])
        saved_subs_format = self.config.get("General", "subtitles_format", fallback="None")
        self.subs_format_combo.setCurrentText(saved_subs_format)
        self.subs_format_combo.currentTextChanged.connect(
            lambda s: self._save_general("subtitles_format", s)
        )
        subs_format_row.addWidget(subs_format_lbl)
        subs_format_row.addWidget(self.subs_format_combo, stretch=1)
        media_group.addLayout(subs_format_row)

        # Chapter embedding
        self.embed_chapters_cb = QCheckBox("Embed chapters")
        self.embed_chapters_cb.setToolTip("Embed chapter markers into the media file when available.")
        embed_chapters_val = self.config.get("General", "embed_chapters", fallback="True")
        self.embed_chapters_cb.setChecked(str(embed_chapters_val) == "True")
        self.embed_chapters_cb.stateChanged.connect(
            lambda s: self._save_general("embed_chapters", str(bool(s)))
        )
        media_group.addWidget(self.embed_chapters_cb)

        updates_group = add_group("Updates")

        # --- yt-dlp Update Section ---
        update_group = QHBoxLayout()

        self.update_channel_combo = QComboBox()
        self.update_channel_combo.addItem("Stable (default)", "stable")
        self.update_channel_combo.addItem("Nightly", "nightly")
        self.update_channel_combo.setToolTip("Choose between stable (recommended) or nightly (cutting-edge) builds of yt-dlp.")
        saved_channel = self.config.get("General", "yt_dlp_update_channel", fallback="stable")
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
        self.setLayout(layout)
    # --- Event handlers ---
    def browse_out(self):
        """Prompt user to choose new output directory."""
        folder = QFileDialog.getExistingDirectory(self, "Select Output Folder")
        if folder:
            folder = os.path.normpath(folder)
            self.config.set("Paths", "completed_downloads_directory", folder)
            self.out_display.setText(folder)
            log.debug(f"Updated output directory: {folder}")
            log.debug("Output directory set by user; temporary directory left unchanged.")

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
        channel = self.update_channel_combo.itemData(index)
        self._save_general("yt_dlp_update_channel", channel)

    def _check_app_update(self):
        """Check GitHub for a newer app release and prompt the user to update."""
        owner = 'vincentwetzel'
        repo = 'MediaDownloader'

        def bg():
            try:
                import core.updater as updater
                rel = updater.get_latest_release(owner, repo)
                if not rel:
                    QTimer.singleShot(0, lambda: QMessageBox.information(self, 'Update Check', 'Could not fetch release information from GitHub.'))
                    return
                tag = rel.get('tag_name') or rel.get('name') or ''
                cmp = updater._compare_versions(APP_VERSION, tag)
                if cmp == -1:
                    QTimer.singleShot(0, lambda: self._prompt_update(rel))
                else:
                    QTimer.singleShot(0, lambda: QMessageBox.information(self, 'Up To Date', f'No update available. Current version: {APP_VERSION}'))
            except Exception as e:
                log.exception('App update check failed')
                QTimer.singleShot(0, lambda: QMessageBox.warning(self, 'Update Check Failed', str(e)))

        t = threading.Thread(target=bg, daemon=True)
        t.start()

    def _prompt_update(self, release_info: dict):
        tag = release_info.get('tag_name') or release_info.get('name') or 'unknown'
        body = release_info.get('body') or ''
        short_body = (body[:2000] + '...') if len(body) > 2000 else body
        text = f"<b>A new version is available: {tag}</b><br><br>{short_body.replace('\n','<br>')}"
        msg = QMessageBox(self)
        msg.setWindowTitle('Update Available')
        msg.setTextFormat(Qt.TextFormat.RichText)
        msg.setText(text)
        yes = msg.addButton('Yes', QMessageBox.ButtonRole.YesRole)
        no = msg.addButton('No', QMessageBox.ButtonRole.NoRole)
        msg.exec()
        if msg.clickedButton() == yes:
            def bg_download():
                try:
                    import core.updater as updater
                    ok, path = updater.check_and_download_update('vincentwetzel', 'MediaDownloader')
                    if not ok:
                        QTimer.singleShot(0, lambda: QMessageBox.warning(self, 'Update Failed', 'Failed to download update.'))
                        return
                    if getattr(sys, 'frozen', False):
                        updater.perform_self_update(path)
                    else:
                        html = release_info.get('html_url')
                        if html:
                            webbrowser.open(html)
                        QTimer.singleShot(0, lambda: QMessageBox.information(self, 'Downloaded', f'Update downloaded to: {path}'))
                except Exception as e:
                    log.exception('Failed to apply update')
                    QTimer.singleShot(0, lambda: QMessageBox.warning(self, 'Update Error', str(e)))

            t = threading.Thread(target=bg_download, daemon=True)
            t.start()

    def _on_version_fetched(self, ver):
        is_nightly = "nightly" in ver.lower() or ".dev" in ver.lower()
        channel_text = " (Nightly)" if is_nightly else " (Stable)"
        if ver == "Unknown":
            channel_text = ""
        self.version_lbl.setText(f"Current version: {ver}{channel_text}")

    def _update_yt_dlp(self):
        """Run yt-dlp -U (or --update-to nightly) in a background thread."""
        import core.yt_dlp_worker
        import shutil
        
        target_exe = core.yt_dlp_worker._YT_DLP_PATH
        if not target_exe:
             core.yt_dlp_worker.check_yt_dlp_available()
             target_exe = core.yt_dlp_worker._YT_DLP_PATH

        if not target_exe:
            target_exe = shutil.which("yt-dlp")
        
        if not target_exe:
            QMessageBox.critical(self, "Update Failed", "Could not locate yt-dlp executable to update.")
            return

        channel = self.update_channel_combo.currentData()
        
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
                creation_flags = 0
                if sys.platform == "win32" and getattr(sys, "frozen", False):
                    creation_flags = subprocess.CREATE_NO_WINDOW
                
                proc = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    creationflags=creation_flags,
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
        
        channel = self.update_channel_combo.currentData()
        channel_name = "Nightly" if channel == "nightly" else "Stable"
        
        msg_lower = message.lower()
        if "pip" in msg_lower and ("installed" in msg_lower or "package manager" in msg_lower):
            self._update_via_pip(channel)
            return
        
        if message.startswith("PIP_UPDATE:"):
            message = message.replace("PIP_UPDATE:", "", 1)
            if success or "requirement already satisfied" in message.lower():
                if "requirement already satisfied" in message.lower():
                     msg_title = "Already Up to Date"
                     msg_body = f"yt-dlp ({channel_name}) is already at the latest version (via pip).\n\nOutput:\n{message}"
                else:
                     msg_title = "Update Successful"
                     msg_body = f"yt-dlp has been updated via pip to the latest {channel_name} version.\n\nOutput:\n{message}"
                
                icon = QMessageBox.Icon.Information
                msg = QMessageBox(self)
                msg.setWindowTitle(msg_title)
                msg.setText(msg_body)
                msg.setIcon(icon)
                msg.exec()
                
                self._refresh_version_label()
            else:
                QMessageBox.warning(self, "Update Failed", f"Pip update command failed.\n\nOutput:\n{message}")
            return

        if success:
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

            self.subs_format_combo.setCurrentText(self.config.get("General", "subtitles_format", fallback="None"))
            
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
            
            idx = self.update_channel_combo.findData("stable")
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
                 msg.setInformativeText('Please check the <a href="https://github.com/yt-dlp/yt-dlp?tab=readme-ov-file#output-template">yt-dlp documentation</a> for proper syntax.')
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
