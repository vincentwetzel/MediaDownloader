import logging
from PyQt6.QtWidgets import (
    QWidget, QMainWindow, QVBoxLayout, QTabWidget,
    QMessageBox
)
from PyQt6.QtCore import Qt, QTimer

from core.config_manager import ConfigManager
from core.download_manager import DownloadManager
from core.playlist_expander import expand_playlist
from ui.tab_start import StartTab
from ui.tab_active import ActiveDownloadsTab
from ui.tab_advanced import AdvancedSettingsTab

log = logging.getLogger(__name__)


class MediaDownloaderApp(QMainWindow):
    def __init__(self):
        super().__init__()
        log.info("Initializing MediaDownloaderApp")

        self.setWindowTitle("Media Downloader")
        self.resize(900, 700)

        # Core components
        self.config_manager = ConfigManager()
        self.download_manager = DownloadManager()

        # UI setup
        self.tabs = QTabWidget()
        self.tab_start = StartTab(self)
        self.tab_active = ActiveDownloadsTab(self)
        self.tab_advanced = AdvancedSettingsTab(self)

        self.tabs.addTab(self.tab_start, "Start a Download")
        self.tabs.addTab(self.tab_active, "Active Downloads")
        self.tabs.addTab(self.tab_advanced, "Advanced Settings")

        main_widget = QWidget()
        layout = QVBoxLayout(main_widget)
        layout.addWidget(self.tabs)
        self.setCentralWidget(main_widget)

        # Connect core events
        self.download_manager.download_added.connect(self.tab_active.add_download_widget)
        self.download_manager.download_finished.connect(self._on_download_finished)
        self.download_manager.download_error.connect(self._on_download_error)

        # Window close handling
        self._downloads_in_progress = False
        self._downloads_failed = []

        # Sync “exit after all downloads complete” flag
        self.exit_after = self.config_manager.get("General", "exit_after", fallback="False") == "True"

        log.debug("Main window setup complete")

    # -------------------------------------------------------------------------
    # DOWNLOAD LOGIC
    # -------------------------------------------------------------------------

    def start_downloads(self, urls, opts):
        """Start downloads for one or multiple URLs."""
        log.info(f"Starting downloads for {len(urls)} item(s)")
        for url in urls:
            expanded = expand_playlist(url)
            for sub_url in expanded:
                self.download_manager.add_download(sub_url, opts)

        self._downloads_in_progress = True

    def _on_download_finished(self, url, success):
        log.info(f"Download finished: {url}, success={success}")
        if not success:
            self._downloads_failed.append(url)

        if self._all_downloads_complete():
            if self.exit_after:
                log.info("All downloads complete; exiting app...")
                QTimer.singleShot(1000, self.close)

    def _on_download_error(self, url, message):
        log.error(f"Download failed: {url}: {message}")
        QMessageBox.warning(self, "Download Error", f"Failed to download:\n{url}\n\n{message}")
        self._downloads_failed.append(url)

    def _all_downloads_complete(self):
        """Returns True if all downloads have finished."""
        active = [d for d in self.download_manager.active_downloads if d.isRunning()]
        return not active

    # -------------------------------------------------------------------------
    # WINDOW CLOSE HANDLING
    # -------------------------------------------------------------------------

    def closeEvent(self, event):
        """Handle window close attempts."""
        active = [d for d in self.download_manager.active_downloads if d.isRunning()]
        if active:
            resp = QMessageBox.question(
                self,
                "Downloads in progress",
                "There are still downloads running.\n\nDo you want to exit anyway?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No
            )
            if resp == QMessageBox.StandardButton.No:
                event.ignore()
                self.tabs.setCurrentWidget(self.tab_active)
                return

        if self._downloads_failed:
            QMessageBox.information(
                self,
                "Failed Downloads",
                f"The following downloads failed:\n\n" + "\n".join(self._downloads_failed)
            )
            self.tabs.setCurrentWidget(self.tab_active)

        super().closeEvent(event)
