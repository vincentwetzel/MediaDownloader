import logging
import sys
from PyQt6.QtWidgets import QApplication, QMainWindow, QTabWidget, QMessageBox
from PyQt6.QtCore import Qt

# Internal modules
from core.config_manager import ConfigManager
from core.thread_pool import ThreadPoolManager
from core.download_worker import DownloadWorker
from core.playlist_utils import expand_playlist
from ui import StartDownloadTab, ActiveDownloadsTab, AdvancedSettingsTab

# Logging setup
logging.basicConfig(
    level=logging.DEBUG,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)]
)
log = logging.getLogger("MediaDownloaderApp")


class MediaDownloaderApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Media Downloader")
        self.resize(900, 650)

        # Core components
        self.config_manager = ConfigManager()
        self.thread_pool = ThreadPoolManager(self.config_manager)

        # Build UI
        self.tabs = QTabWidget()
        self.setCentralWidget(self.tabs)
        self._build_tabs()

        # Track running downloads
        self.downloads = []
        log.debug("Application initialized")

    def _build_tabs(self):
        """Attach the three main tabs to the window."""
        self.tab_start = StartDownloadTab(self)
        self.tab_active = ActiveDownloadsTab(self)
        self.tab_advanced = AdvancedSettingsTab(self)

        self.tabs.addTab(self.tab_start, "Start a Download")
        self.tabs.addTab(self.tab_active, "Active Downloads")
        self.tabs.addTab(self.tab_advanced, "Advanced Settings")

    def closeEvent(self, event):
        """Ask for confirmation if downloads are active."""
        if self.thread_pool.has_active_downloads():
            resp = QMessageBox.question(
                self,
                "Downloads in Progress",
                "Some downloads are still running. Do you really want to exit?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if resp == QMessageBox.StandardButton.No:
                event.ignore()
                return
        log.info("Shutting down application.")
        self.thread_pool.shutdown()
        event.accept()


def main():
    app = QApplication(sys.argv)
    wnd = MediaDownloaderApp()
    wnd.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
