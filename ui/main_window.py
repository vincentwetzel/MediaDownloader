import logging
from PyQt6.QtWidgets import (
    QWidget, QMainWindow, QVBoxLayout, QTabWidget,
    QMessageBox
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal

from core.config_manager import ConfigManager
from core.download_manager import DownloadManager
from core.playlist_expander import expand_playlist
from ui.tab_start import StartTab
from ui.tab_active import ActiveDownloadsTab
from ui.tab_advanced import AdvancedSettingsTab
import threading

log = logging.getLogger(__name__)


class MediaDownloaderApp(QMainWindow):
    add_download_request = pyqtSignal(str, object)
    def __init__(self):
        super().__init__()
        log.info("Initializing MediaDownloaderApp")

        self.setWindowTitle("Media Downloader")
        self.resize(900, 700)

        # Core components
        self.config_manager = ConfigManager()
        self.download_manager = DownloadManager()
        # Ensure the download manager uses the same config instance so
        # runtime changes to settings (e.g. max_threads) are seen immediately.
        try:
            self.download_manager.config = self.config_manager
        except Exception:
            pass

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
        # Signal used to request adding downloads from background threads
        try:
            self.add_download_request.connect(self._handle_add_download_request)
        except Exception:
            pass

        # Window close handling
        self._downloads_in_progress = False
        self._downloads_failed = []

        # Sync “exit after all downloads complete” flag — always default to False on startup
        self.exit_after = False

        log.debug("Main window setup complete")

    # -------------------------------------------------------------------------
    # DOWNLOAD LOGIC
    # -------------------------------------------------------------------------

    def start_downloads(self, urls, opts):
        """Start downloads for one or multiple URLs."""
        log.info(f"Starting downloads for {len(urls)} item(s)")
        # Immediately create UI placeholders for each provided URL so the
        # Active Downloads tab shows entries with no delay.
        try:
            for url in urls:
                try:
                    self.tab_active.add_download_widget(url)
                except Exception:
                    log.debug(f"Failed to add immediate placeholder for {url}", exc_info=True)
            # Switch to Active Downloads tab immediately
            try:
                self.tabs.setCurrentWidget(self.tab_active)
            except Exception:
                log.debug("Failed to switch to Active Downloads tab immediately", exc_info=True)
            # Mark downloads in progress
            self._downloads_in_progress = True
        except Exception:
            log.debug("Immediate placeholder creation failed", exc_info=True)

        # Run playlist expansion and add_download calls in a background thread
        # to avoid blocking the GUI when expanding or processing many URLs.
        def _bg_worker():
            any_added = False
            for url in urls:
                try:
                    expanded = expand_playlist(url)
                except Exception:
                    expanded = [url]
                for sub_url in expanded:
                    # Request add_download on the GUI thread via signal (thread-safe)
                    try:
                        self.add_download_request.emit(sub_url, opts)
                    except Exception:
                        # As a last resort, try scheduling via QTimer
                        QTimer.singleShot(0, lambda su=sub_url: self.download_manager.add_download(su, opts))
                    any_added = True
            # Mark as downloads in progress (set on GUI thread via signal)
            if any_added:
                try:
                    self.add_download_request.emit('__mark_in_progress__', None)
                except Exception:
                    QTimer.singleShot(0, lambda: setattr(self, '_downloads_in_progress', True))

        try:
            t = threading.Thread(target=_bg_worker, daemon=True)
            t.start()
        except Exception:
            # Fallback to synchronous behavior if thread cannot be started
            log.exception("Failed to start background thread for downloads; falling back to synchronous start")
            added_any = False
            for url in urls:
                expanded = expand_playlist(url)
                for sub_url in expanded:
                    self.download_manager.add_download(sub_url, opts)
                    added_any = True
            self._downloads_in_progress = True
            if added_any:
                try:
                    self.tabs.setCurrentWidget(self.tab_active)
                except Exception:
                    log.exception("Failed to switch to Active Downloads tab")

    def _handle_add_download_request(self, url, opts):
        """Slot invoked in GUI thread to actually add a download or update state.

        Special-case URL '__mark_in_progress__' to set internal flag and switch tab.
        """
        if url == '__mark_in_progress__':
            try:
                self._downloads_in_progress = True
                self.tabs.setCurrentWidget(self.tab_active)
            except Exception:
                pass
            return
        try:
            self.download_manager.add_download(url, opts)
        except Exception:
            log.exception(f"Failed to add download for {url}")

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

    def cancel_download(self, url):
        """Cancel a download by URL."""
        log.info(f"Cancelling download: {url}")
        for worker in self.download_manager.active_downloads:
            if hasattr(worker, 'url') and worker.url == url:
                if worker.isRunning():
                    worker.cancel()
                    log.info(f"Download cancelled: {url}")
                    # Update UI to reflect cancellation
                    try:
                        # Try to obtain a clean title from the worker if available
                        title = getattr(worker, 'video_title', None) or getattr(worker, 'title', None) or url
                        # Ask the Active tab to mark cancelled
                        try:
                            self.tab_active.mark_cancelled(url, title)
                        except Exception:
                            pass
                    except Exception:
                        pass
                return
        log.warning(f"Download not found for cancellation: {url}")

    def retry_download(self, url):
        """Retry a failed download by URL."""
        log.info(f"Retrying download: {url}")
        # Use stored original options when available so retry behaves like original
        opts = self.download_manager._original_opts.get(url, [])
        self.download_manager.add_download(url, opts)

    def resume_download(self, url):
        """Resume a cancelled download (currently implemented as restart)."""
        log.info(f"Resuming download: {url}")
        # Use original options where possible so yt-dlp can attempt to resume
        opts = self.download_manager._original_opts.get(url, [])
        self.download_manager.add_download(url, opts)

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
