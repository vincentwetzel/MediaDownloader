import logging
from PyQt6.QtWidgets import (
    QWidget, QMainWindow, QVBoxLayout, QTabWidget,
    QMessageBox
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal

from core.config_manager import ConfigManager
from core.download_manager import DownloadManager
from core.playlist_expander import expand_playlist, is_likely_playlist
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
        """Start downloads for one or multiple URLs, handling playlist logic."""
        log.info(f"Starting downloads for {len(urls)} item(s) with opts: {opts}")
        self.tabs.setCurrentWidget(self.tab_active)
        self._downloads_in_progress = True

        playlist_mode = opts.get("playlist_mode", "Ask")

        for url in urls:
            is_playlist = is_likely_playlist(url)

            if is_playlist and playlist_mode == "Ask":
                msg_box = QMessageBox(self)
                msg_box.setWindowTitle("Playlist Detected")
                msg_box.setText(f"The URL you provided appears to be a playlist:\n\n{url}")
                msg_box.setInformativeText("Do you want to download the entire playlist or just the single video?")
                btn_all = msg_box.addButton("Download All", QMessageBox.ButtonRole.YesRole)
                btn_one = msg_box.addButton("Download Single", QMessageBox.ButtonRole.NoRole)
                msg_box.addButton("Cancel", QMessageBox.ButtonRole.RejectRole)
                msg_box.exec()

                clicked_button = msg_box.clickedButton()
                if clicked_button == btn_all:
                    # Download All
                    self.tab_active.add_download_widget(url)
                    self.tab_active.set_placeholder_message(url, "Preparing playlist download...")
                    self._start_background_download_processing([url], opts, expand_playlists=True)
                elif clicked_button == btn_one:
                    # Download Single
                    self.tab_active.add_download_widget(url)
                    self._start_background_download_processing([url], opts, expand_playlists=False)
                else:
                    # Cancel
                    log.info(f"Playlist download cancelled by user for URL: {url}")

            elif is_playlist and "single" in playlist_mode.lower():
                # Download Single (ignore playlist)
                self.tab_active.add_download_widget(url)
                self._start_background_download_processing([url], opts, expand_playlists=False)

            else:
                # Download All (or not a playlist)
                self.tab_active.add_download_widget(url)
                if is_playlist:
                     self.tab_active.set_placeholder_message(url, "Preparing playlist download...")
                self._start_background_download_processing([url], opts, expand_playlists=True)


    def _start_background_download_processing(self, urls, opts, expand_playlists=True):
        """Helper to run playlist expansion and download queuing in a background thread."""
        def _bg_worker():
            for url in urls:
                urls_to_download = []
                if expand_playlists:
                    try:
                        expanded = expand_playlist(url)
                        # If playlist, update UI
                        if len(expanded) > 1:
                            QTimer.singleShot(0, lambda u=url, c=len(expanded): self.tab_active.set_placeholder_message(u, f"Calculating playlist ({c} items)..."))
                            QTimer.singleShot(0, lambda u=url, e=expanded: self.tab_active.replace_placeholder_with_entries(u, e))
                        urls_to_download.extend(expanded)
                    except Exception:
                        urls_to_download.append(url)
                else:
                    urls_to_download.append(url)

                for sub_url in urls_to_download:
                    QTimer.singleShot(0, lambda su=sub_url, o=opts: self.download_manager.add_download(su, o))

            QTimer.singleShot(0, lambda: setattr(self, '_downloads_in_progress', True))

        thread = threading.Thread(target=_bg_worker, daemon=True)
        thread.start()

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
        if url == '__playlist_expanded__':
            try:
                orig, expanded = opts
                self.tab_active.replace_placeholder_with_entries(orig, expanded)
            except Exception:
                log.exception("Failed to replace playlist placeholder with entries")
            return
        if url == '__playlist_detected__':
            try:
                orig, count = opts
                # Update the existing placeholder to a calculating message
                self.tab_active.set_placeholder_message(orig, f"Calculating playlist ({count} items)...")
            except Exception:
                log.exception("Failed to set playlist calculating message")
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
