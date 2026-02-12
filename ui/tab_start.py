import logging
import subprocess
import shutil
import platform
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QGridLayout,
    QPushButton, QLabel, QComboBox, QCheckBox, QDialog, QTextEdit
)
from PyQt6.QtCore import Qt
from ui.widgets import UrlTextEdit

log = logging.getLogger(__name__)


class StartTab(QWidget):
    def __init__(self, main_window):
        super().__init__()
        self.main = main_window
        self.config = main_window.config_manager
        self._build_tab_start()

    def _build_tab_start(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(8, 8, 8, 8)

        # Open downloads folder
        top_row = QHBoxLayout()
        top_row.addStretch()
        self.open_downloads_btn_start = QPushButton("Open Downloads Folder")
        self.open_downloads_btn_start.setFixedHeight(36)
        self.open_downloads_btn_start.clicked.connect(self._on_open_downloads_clicked)
        self.open_downloads_btn_start.setToolTip("Open the configured output/downloads folder.")
        top_row.addWidget(self.open_downloads_btn_start)
        layout.addLayout(top_row)

        # URL input
        url_label = QLabel("Video/Playlist URL(s):")
        url_label.setToolTip("Enter one or more URLs (one per line). Any site supported by yt-dlp is supported.")
        self.url_input = UrlTextEdit()
        self.url_input.setPlaceholderText("Paste one or more media URLs (one per line)...")

        self.download_btn = QPushButton("Download")
        self.download_btn.setToolTip("Start downloading the URLs above.")
        self.download_btn.clicked.connect(self._on_download_clicked)
        
        # New download type dropdown
        self.download_type_combo = QComboBox()
        self.download_type_combo.addItems(["Video", "Audio Only", "Gallery", "View Formats"])
        self.download_type_combo.setToolTip("Choose whether to download the full video, just the audio, image gallery, or view available formats for the URL.")
        # Default to "Video" on first launch
        self.download_type_combo.setCurrentText("Video")
        # Update button text when download type changes
        self.download_type_combo.currentTextChanged.connect(self._on_download_type_changed)
        # Initialize button text to match the dropdown's default value
        self._on_download_type_changed("Video")
        # We can link this to config if we want to persist the choice, e.g.:
        # self.download_type_combo.setCurrentText(self.config.get("General", "download_type", "Video"))
        # self.download_type_combo.currentTextChanged.connect(lambda v: self.config.set("General", "download_type", v))

        # Right column with Download button and new dropdown
        right_col_layout = QVBoxLayout()
        right_col_layout.addWidget(self.download_btn)

        # Download type dropdown with label
        download_type_layout = QHBoxLayout()
        download_type_label = QLabel("Download Type:")
        download_type_layout.addWidget(download_type_label)
        download_type_layout.addWidget(self.download_type_combo)
        right_col_layout.addLayout(download_type_layout)
        
        # Adjust button heights to fill space proportionally
        self.download_btn.setSizePolicy(
            self.download_btn.sizePolicy().horizontalPolicy(),
            self.download_btn.sizePolicy().verticalPolicy()
        )
        self.download_type_combo.setFixedHeight(30) 
        total_right_col_height = 120
        self.download_btn.setFixedHeight(total_right_col_height - 30)

        # URL row now contains the URL input and the right column
        url_row = QHBoxLayout()
        url_row.addWidget(self.url_input, stretch=1)
        url_row.addLayout(right_col_layout)

        # Set the url_input height to match the right column total height
        self.url_input.setFixedHeight(total_right_col_height)

        # --- Video + Audio settings ---
        groups_row = QHBoxLayout()

        # Video settings
        vg = QGroupBox("Video Settings")
        vg_layout = QGridLayout(vg)

        self.video_quality_combo = QComboBox()
        self.video_quality_combo.addItems(["best", "2160p", "1440p", "1080p", "720p", "480p"])
        self.video_quality_combo.setCurrentText(self.config.get("General", "video_quality", "best"))
        self.video_quality_combo.setToolTip("Preferred video quality.")
        self.video_quality_combo.currentTextChanged.connect(lambda v: self.config.set("General", "video_quality", v))

        self.video_ext_combo = QComboBox()
        for ex in ("mp4", "mkv", "webm", "avi", "flv", "mov"):
            self.video_ext_combo.addItem(ex, ex)
        cur_ext = self.config.get("General", "video_ext", "mp4")
        if cur_ext:
            self.video_ext_combo.setCurrentText(cur_ext)
        self.video_ext_combo.setToolTip("Preferred video file format.")
        self.video_ext_combo.currentTextChanged.connect(
            lambda v: self.config.set("General", "video_ext", v or "")
        )

        self.vcodec_combo = QComboBox()
        for c in ("h264", "h265", "vp9", "av1"):
            self.vcodec_combo.addItem(c, c)
        cur_codec = self.config.get("General", "vcodec", "h264")
        if cur_codec:
            self.vcodec_combo.setCurrentText(cur_codec)
        self.vcodec_combo.setToolTip("Preferred video codec (e.g., h264).")
        self.vcodec_combo.currentTextChanged.connect(
            lambda v: self.config.set("General", "vcodec", v or "")
        )

        vg_layout.addWidget(QLabel("Quality:"), 0, 0)
        vg_layout.addWidget(self.video_quality_combo, 0, 1)
        vg_layout.addWidget(QLabel("Extension:"), 1, 0)
        vg_layout.addWidget(self.video_ext_combo, 1, 1)
        vg_layout.addWidget(QLabel("Codec:"), 2, 0)
        vg_layout.addWidget(self.vcodec_combo, 2, 1)

        # Audio settings
        ag = QGroupBox("Audio Settings")
        ag_layout = QGridLayout(ag)

        self.audio_quality_combo = QComboBox()
        for label, val in [("best", "best"), ("64 KB", "64k"), ("128 KB", "128k"),
                           ("192 KB", "192k"), ("256 KB", "256k"), ("320 KB", "320k")]:
            self.audio_quality_combo.addItem(label, val)
        curaq = self.config.get("General", "audio_quality", "best")
        for i in range(self.audio_quality_combo.count()):
            if self.audio_quality_combo.itemData(i) == curaq:
                self.audio_quality_combo.setCurrentIndex(i)
                break
        self.audio_quality_combo.setToolTip("Preferred audio bitrate.")
        self.audio_quality_combo.currentIndexChanged.connect(
            lambda idx: self.config.set("General", "audio_quality", self.audio_quality_combo.itemData(idx))
        )

        self.audio_ext_combo = QComboBox()
        for ex in ("mp3", "m4a", "opus", "aac", "flac", "alac", "vorbis", "wav"):
            self.audio_ext_combo.addItem(ex, ex)
        cura = self.config.get("General", "audio_ext", "mp3")
        if cura:
            self.audio_ext_combo.setCurrentText(cura)
        self.audio_ext_combo.setToolTip("Preferred audio file format.")
        self.audio_ext_combo.currentTextChanged.connect(
            lambda v: self.config.set("General", "audio_ext", v or "")
        )

        self.acodec_combo = QComboBox()
        for a in ("aac", "opus", "mp3", "vorbis", "flac", "alac"):
            self.acodec_combo.addItem(a, a)
        curaud = self.config.get("General", "acodec", "aac")
        if curaud:
            self.acodec_combo.setCurrentText(curaud)
        self.acodec_combo.setToolTip("Preferred audio codec.")
        self.acodec_combo.currentTextChanged.connect(
            lambda v: self.config.set("General", "acodec", v or "")
        )

        ag_layout.addWidget(QLabel("Quality:"), 0, 0)
        ag_layout.addWidget(self.audio_quality_combo, 0, 1)
        ag_layout.addWidget(QLabel("Extension:"), 1, 0)
        ag_layout.addWidget(self.audio_ext_combo, 1, 1)
        ag_layout.addWidget(QLabel("Codec:"), 2, 0)
        ag_layout.addWidget(self.acodec_combo, 2, 1)

        groups_row.addWidget(vg)
        groups_row.addWidget(ag)

        # --- bottom controls ---
        bottom = QGridLayout()
        playlist_lbl = QLabel("Playlist:")
        playlist_lbl.setToolTip("Choose how to handle URLs that contain playlists.")
        self.playlist_mode = QComboBox()
        self.playlist_mode.addItems(["Ask", "Download All (no prompt)", "Download Single (ignore playlist)"])
        self.playlist_mode.setToolTip("Choose how to handle playlists.")

        max_lbl = QLabel("Max Concurrent:")
        max_lbl.setToolTip("Set the maximum number of concurrent downloads (limited to 4 on app startup).")
        self.max_threads_combo = QComboBox()
        self.max_threads_combo.addItems([str(i) for i in range(1, 9)])
        
        # Load max_threads from config, but cap it at 4 when loading
        saved_threads = self.config.get("General", "max_threads", "2")
        try:
            threads_val = int(saved_threads)
            if threads_val > 4:
                threads_val = 4
                self.config.set("General", "max_threads", str(threads_val))
            self.max_threads_combo.setCurrentText(str(threads_val))
        except (ValueError, TypeError):
            self.max_threads_combo.setCurrentText("2")
        
        self.max_threads_combo.setToolTip("Maximum concurrent downloads.")
        def _on_max_changed(t):
            # Persist the new value (capped at 4 for persistence) and notify the download manager so it
            # can immediately start more downloads if the limit increased.
            # Note: Users can temporarily run more than 4 in the current session, but on restart it will revert to 4.
            try:
                threads_val = int(t)
                # Cap at 4 before saving to config for app restart
                if threads_val > 4:
                    threads_val = 4
                self.config.set("General", "max_threads", str(threads_val))
            except Exception:
                pass
            try:
                dm = getattr(self.main, 'download_manager', None)
                if dm:
                    dm._maybe_start_next()
            except Exception:
                pass

        self.max_threads_combo.currentTextChanged.connect(_on_max_changed)

        # Add rate limit dropdown
        rate_limit_lbl = QLabel("Rate Limit:")
        rate_limit_lbl.setToolTip("Limit the download speed for each individual download (e.g., 5M for 5 MB/s).")
        self.rate_limit_combo = QComboBox()
        self.rate_limit_combo.addItem("Unlimited", None)
        self.rate_limit_combo.addItem("10 MB/s", "10M")
        self.rate_limit_combo.addItem("5 MB/s", "5M")
        self.rate_limit_combo.addItem("2 MB/s", "2M")
        self.rate_limit_combo.addItem("1 MB/s", "1M")
        self.rate_limit_combo.addItem("500 KB/s", "500K")
        self.rate_limit_combo.addItem("250 KB/s", "250K")
        
        saved_rate_limit = self.config.get("General", "rate_limit")
        if saved_rate_limit:
            index = self.rate_limit_combo.findData(saved_rate_limit)
            if index != -1:
                self.rate_limit_combo.setCurrentIndex(index)
        
        self.rate_limit_combo.currentIndexChanged.connect(
            lambda idx: self.config.set("General", "rate_limit", self.rate_limit_combo.itemData(idx))
        )

        self.exit_after_cb = QCheckBox("Exit after all downloads complete")
        # Always start unchecked on app launch; do not persist this checkbox's state.
        self.exit_after_cb.setChecked(False)
        self.exit_after_cb.setToolTip("Automatically close app after all downloads finish.")
        # Update runtime flag on change, but don't save to config file.
        # Use boolean conversion of the state (non-zero => checked) to avoid Qt enum issues.
        self.exit_after_cb.stateChanged.connect(
            lambda s: setattr(self.main, "exit_after", (s != 0)))

        bottom.addWidget(playlist_lbl, 0, 0)
        bottom.addWidget(self.playlist_mode, 0, 1)
        bottom.addWidget(max_lbl, 0, 2)
        bottom.addWidget(self.max_threads_combo, 0, 3)

        bottom.addWidget(rate_limit_lbl, 1, 0)
        bottom.addWidget(self.rate_limit_combo, 1, 1)
        bottom.addWidget(self.exit_after_cb, 1, 3, 1, 2)

        layout.addWidget(url_label)
        layout.addLayout(url_row)
        layout.addLayout(groups_row)
        layout.addLayout(bottom)
        layout.addStretch()
        self.setLayout(layout)

    # --- actions ---
    def _on_download_clicked(self):
        from PyQt6.QtWidgets import QMessageBox
        from utils.validators import is_search_url

        raw = self.url_input.toPlainText().strip()
        if not raw:
            QMessageBox.information(self, "No URL", "Please paste at least one URL.")
            return

        urls = [line.strip() for line in raw.splitlines() if line.strip()]
        if not urls:
            return

        # Filter out search URLs
        valid_urls = []
        search_urls_found = []
        for url in urls:
            if is_search_url(url):
                search_urls_found.append(url)
            else:
                valid_urls.append(url)

        # If any search URLs were found, show a warning
        if search_urls_found:
            bad_urls_str = "\n".join(f"- {u}" for u in search_urls_found)
            QMessageBox.warning(
                self,
                "Invalid URL",
                f"The following URLs appear to be search pages and cannot be downloaded:\n{bad_urls_str}\n\nPlease provide direct links to videos or playlists."
            )

        # If there are no valid URLs left, stop here
        if not valid_urls:
            return

        # Handle "View Formats" mode
        if self.download_type_combo.currentText() == "View Formats":
            self._show_formats_dialog(valid_urls)
            return

        opts = {
            "audio_only": self.download_type_combo.currentText() == "Audio Only",
            "use_gallery_dl": self.download_type_combo.currentText() == "Gallery",
            "video_quality": self.video_quality_combo.currentText(),
            "video_ext": self.video_ext_combo.currentText() or "",
            "vcodec": self.vcodec_combo.currentText() or "",
            "audio_quality": self.audio_quality_combo.itemData(self.audio_quality_combo.currentIndex()) or "best",
            "audio_ext": self.audio_ext_combo.currentText() or "",
            "acodec": self.acodec_combo.currentText() or "",
            "playlist_mode": self.playlist_mode.currentText(),
            "rate_limit": self.rate_limit_combo.itemData(self.rate_limit_combo.currentIndex()),
        }
        self.main.start_downloads(valid_urls, opts)

    def _on_download_type_changed(self, text):
        """Update button text and tooltip based on the selected download type."""
        if text == "View Formats":
            self.download_btn.setText("View Formats")
            self.download_btn.setToolTip("Display available download formats for the URL.")
        elif text == "Audio Only":
            self.download_btn.setText("Download Audio")
            self.download_btn.setToolTip("Start downloading audio from the URLs above.")
        elif text == "Gallery":
            self.download_btn.setText("Download Gallery")
            self.download_btn.setToolTip("Start downloading image gallery from the URLs above.")
        else:  # "Video"
            self.download_btn.setText("Download Video")
            self.download_btn.setToolTip("Start downloading video from the URLs above.")

    def reset_to_defaults(self):
        # Video settings
        self.video_quality_combo.setCurrentText(self.config.get("General", "video_quality", "best"))
        self.video_ext_combo.setCurrentText(self.config.get("General", "video_ext", "mp4"))
        self.vcodec_combo.setCurrentText(self.config.get("General", "vcodec", "h264"))

        # Audio settings
        curaq = self.config.get("General", "audio_quality", "best")
        for i in range(self.audio_quality_combo.count()):
            if self.audio_quality_combo.itemData(i) == curaq:
                self.audio_quality_combo.setCurrentIndex(i)
                break
        self.audio_ext_combo.setCurrentText(self.config.get("General", "audio_ext", "mp3"))
        self.acodec_combo.setCurrentText(self.config.get("General", "acodec", "aac"))

        # Rate limit
        saved_rate_limit = self.config.get("General", "rate_limit")
        if saved_rate_limit:
            index = self.rate_limit_combo.findData(saved_rate_limit)
            if index != -1:
                self.rate_limit_combo.setCurrentIndex(index)
            else:
                self.rate_limit_combo.setCurrentIndex(0)
        else:
            self.rate_limit_combo.setCurrentIndex(0)

    def _on_open_downloads_clicked(self):
        """Safely open the downloads folder via Advanced tab."""
        adv = getattr(self.main, "tab_advanced", None)
        if adv and hasattr(adv, "open_downloads_folder"):
            adv.open_downloads_folder()
        else:
            from PyQt6.QtWidgets import QMessageBox
            QMessageBox.warning(
                self,
                "Unavailable",
                "Advanced Settings are not yet initialized. Please try again after the app finishes loading."
            )

    def _show_formats_dialog(self, urls):
        """Display available download formats for the given URLs using yt-dlp -F"""
        from PyQt6.QtWidgets import QMessageBox
        from core.yt_dlp_worker import check_yt_dlp_available, _YT_DLP_PATH
        import shutil
        import sys
        
        if len(urls) > 1:
            QMessageBox.information(
                self,
                "Multiple URLs",
                "Please select one URL at a time to view formats. Showing formats for the first URL only."
            )
            urls = urls[:1]
        
        url = urls[0]
        
        # Find yt-dlp executable
        yt_dlp_cmd = None
        if _YT_DLP_PATH:
            yt_dlp_cmd = _YT_DLP_PATH
        else:
            yt_dlp_cmd = shutil.which("yt-dlp") or "yt-dlp"
        
        # Hide console window on Windows for compiled builds
        creation_flags = 0
        if sys.platform == "win32" and getattr(sys, "frozen", False):
            creation_flags = subprocess.CREATE_NO_WINDOW
        
        try:
            # Run yt-dlp -F to get available formats
            cmd = [yt_dlp_cmd, "-F", url]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30,
                creationflags=creation_flags
            )
            
            if result.returncode != 0:
                error_msg = result.stderr or result.stdout or "Unknown error"
                QMessageBox.critical(
                    self,
                    "Format Query Failed",
                    f"Failed to retrieve formats for the URL:\n\n{error_msg}"
                )
                return
            
            # Create and display a dialog with the formats
            dialog = QDialog(self)
            dialog.setWindowTitle(f"Available Formats - {url}")
            dialog.setGeometry(100, 100, 900, 600)
            
            layout = QVBoxLayout()
            
            text_edit = QTextEdit()
            text_edit.setReadOnly(True)
            text_edit.setPlainText(result.stdout)
            text_edit.setFont(text_edit.font())  # Use monospace would be better, but using default
            
            close_btn = QPushButton("Close")
            close_btn.clicked.connect(dialog.accept)
            
            layout.addWidget(QLabel("Available formats for this URL:"))
            layout.addWidget(text_edit)
            layout.addWidget(close_btn)
            
            dialog.setLayout(layout)
            dialog.exec()
            
        except subprocess.TimeoutExpired:
            QMessageBox.critical(
                self,
                "Timeout",
                "The format query timed out. The URL may be unreachable or yt-dlp may be unresponsive."
            )
        except FileNotFoundError:
            QMessageBox.critical(
                self,
                "yt-dlp Not Found",
                "Could not find yt-dlp executable. Please check your installation."
            )
        except Exception as e:
            QMessageBox.critical(
                self,
                "Error",
                f"An error occurred while querying formats:\n\n{str(e)}"
            )
