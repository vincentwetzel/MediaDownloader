import logging
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QGridLayout,
    QPushButton, QLabel, QComboBox, QCheckBox
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
        self.url_input.setFixedHeight(120)
        self.url_input.setPlaceholderText("Paste one or more media URLs (one per line)...")

        self.download_btn = QPushButton("Download")
        self.download_btn.setFixedHeight(120)
        self.download_btn.setFixedWidth(180)
        self.download_btn.setToolTip("Start downloading the URLs above.")
        self.download_btn.clicked.connect(self._on_download_clicked)

        url_row = QHBoxLayout()
        url_row.addWidget(self.url_input, stretch=1)
        url_row.addWidget(self.download_btn)

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
        self.video_ext_combo.addItem("default (yt-dlp)", "")
        for ex in ("mp4", "mkv", "webm"):
            self.video_ext_combo.addItem(ex, ex)
        cur_ext = self.config.get("General", "video_ext", "")
        if cur_ext:
            idx = self.video_ext_combo.findData(cur_ext)
            if idx >= 0:
                self.video_ext_combo.setCurrentIndex(idx)
        self.video_ext_combo.setToolTip("Preferred video file format.")
        self.video_ext_combo.currentIndexChanged.connect(
            lambda _: self.config.set("General", "video_ext", self.video_ext_combo.itemData(i) or "")
        )

        self.vcodec_combo = QComboBox()
        self.vcodec_combo.addItem("default (yt-dlp)", "")
        for c in ("h264", "h265", "vp9"):
            self.vcodec_combo.addItem(c, c)
        cur_codec = self.config.get("General", "video_codec", "")
        if cur_codec:
            idx = self.vcodec_combo.findData(cur_codec)
            if idx >= 0:
                self.vcodec_combo.setCurrentIndex(idx)
        self.vcodec_combo.setToolTip("Preferred video codec (e.g., h264).")
        self.vcodec_combo.currentIndexChanged.connect(
            lambda _: self.config.set("General", "video_codec", self.vcodec_combo.itemData(i) or "")
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
            lambda _: self.config.set("General", "audio_quality", self.audio_quality_combo.itemData(i))
        )

        self.audio_ext_combo = QComboBox()
        self.audio_ext_combo.addItem("default (yt-dlp)", "")
        for ex in ("mp3", "m4a", "opus", "aac", "flac"):
            self.audio_ext_combo.addItem(ex, ex)
        cura = self.config.get("General", "audio_ext", "")
        if cura:
            ia = self.audio_ext_combo.findData(cura)
            if ia >= 0:
                self.audio_ext_combo.setCurrentIndex(ia)
        self.audio_ext_combo.setToolTip("Preferred audio file format.")
        self.audio_ext_combo.currentIndexChanged.connect(
            lambda _: self.config.set("General", "audio_ext", self.audio_ext_combo.itemData(i) or "")
        )

        self.acodec_combo = QComboBox()
        self.acodec_combo.addItem("default (yt-dlp)", "")
        for a in ("aac", "opus", "mp3", "vorbis", "flac"):
            self.acodec_combo.addItem(a, a)
        curaud = self.config.get("General", "audio_codec", "")
        if curaud:
            ia = self.acodec_combo.findData(curaud)
            if ia >= 0:
                self.acodec_combo.setCurrentIndex(ia)
        self.acodec_combo.setToolTip("Preferred audio codec.")
        self.acodec_combo.currentIndexChanged.connect(
            lambda _: self.config.set("General", "audio_codec", self.acodec_combo.itemData(i) or "")
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
        self.playlist_mode = QComboBox()
        self.playlist_mode.addItems(["Ask", "Download All (no prompt)", "Download Single (ignore playlist)"])
        self.playlist_mode.setToolTip("Choose how to handle playlists.")

        max_lbl = QLabel("Max concurrent:")
        self.max_threads_combo = QComboBox()
        self.max_threads_combo.addItems(["1", "2", "3", "4"])
        self.max_threads_combo.setCurrentText(self.config.get("General", "max_threads", "2"))
        self.max_threads_combo.setToolTip("Maximum concurrent downloads.")
        self.max_threads_combo.currentTextChanged.connect(lambda t: self.config.set("General", "max_threads", t))

        self.audio_only_cb = QCheckBox("Download audio only")
        self.audio_only_cb.setChecked(False)
        self.audio_only_cb.setToolTip("If checked, download only audio.")
        self.exit_after_cb = QCheckBox("Exit after all downloads complete")
        self.exit_after_cb.setChecked(self.config.get("General", "exit_after", "False") == "True")
        self.exit_after_cb.setToolTip("Automatically close app after all downloads finish.")
        self.exit_after_cb.stateChanged.connect(
            lambda s: self.config.set("General", "exit_after", "True" if s else "False"))

        bottom.addWidget(playlist_lbl, 0, 0)
        bottom.addWidget(self.playlist_mode, 0, 1)
        bottom.addWidget(max_lbl, 0, 2)
        bottom.addWidget(self.max_threads_combo, 0, 3)
        bottom.addWidget(self.audio_only_cb, 0, 4)
        bottom.addWidget(self.exit_after_cb, 1, 4)

        layout.addWidget(url_label)
        layout.addLayout(url_row)
        layout.addLayout(groups_row)
        layout.addLayout(bottom)
        layout.addStretch()
        self.setLayout(layout)

    # --- actions ---
    def _on_download_clicked(self):
        raw = self.url_input.toPlainText().strip()
        if not raw:
            from PyQt6.QtWidgets import QMessageBox
            QMessageBox.information(self, "No URL", "Please paste at least one URL.")
            return
        urls = [line.strip() for line in raw.splitlines() if line.strip()]
        if not urls:
            return
        opts = {
            "audio_only": self.audio_only_cb.isChecked(),
        }
        self.main.start_downloads(urls, opts)

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
