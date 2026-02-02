import logging
import os
import html
from PyQt6.QtWidgets import (
    QWidget, QMainWindow, QVBoxLayout, QTabWidget,
    QMessageBox
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QUrl
from PyQt6.QtGui import QDesktopServices

from core.config_manager import ConfigManager
from core.download_manager import DownloadManager
from core.archive_manager import ArchiveManager
from core.playlist_expander import expand_playlist, is_likely_playlist, PlaylistExpansionError
from ui.tab_start import StartTab
from ui.tab_active import ActiveDownloadsTab
from ui.tab_advanced import AdvancedSettingsTab
import threading
from core import extractor_index

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
        self.archive_manager = ArchiveManager()
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
        self.download_manager.video_quality_warning.connect(self._on_video_quality_warning)
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

        # Check for output directory on startup
        self._check_output_directory()

        log.debug("Main window setup complete")

        # Start building extractor index in background for fast host checks
        try:
            extractor_index.build_index_async()
        except Exception:
            log.debug("Extractor index background build not started")

        # Check for app updates shortly after startup (non-blocking)
        try:
            # Only run startup check if user has enabled it in config (default True)
            auto_check = self.config_manager.get("General", "auto_check_updates", fallback="True")
            if str(auto_check) == "True":
                QTimer.singleShot(2000, self._check_for_app_update)
        except Exception:
            log.debug("Startup update check not scheduled")

    def _check_output_directory(self):
        """Check if output directory is set, if not, prompt user."""
        out_path = self.config_manager.get("Paths", "completed_downloads_directory", fallback="")
        if not out_path:
            # If no output path, prompt user to select one
            # We use a timer to let the window show first
            QTimer.singleShot(500, self._prompt_for_output_dir)
        
        # Set default temp directory if not set
        temp_path = self.config_manager.get("Paths", "temporary_downloads_directory", fallback="")
        # Do not create or set a default temporary directory here. Leave it unset
        # until the user explicitly chooses one in Advanced Settings to avoid
        # creating files/folders in the application directory unexpectedly.

    def _prompt_for_output_dir(self):
        """Prompt user to select an output directory."""
        msg = QMessageBox(self)
        msg.setWindowTitle("Setup Required")
        msg.setText("Please select a folder where downloaded files will be saved.")
        msg.setIcon(QMessageBox.Icon.Information)
        msg.exec()
        
        # Switch to advanced tab so they see where it is being set
        self.tabs.setCurrentWidget(self.tab_advanced)
        self.tab_advanced.browse_out()
        
        # Check again if they set it
        out_path = self.config_manager.get("Paths", "completed_downloads_directory", fallback="")
        if not out_path:
             QMessageBox.warning(self, "Setup Incomplete", "You must select an output folder before downloading.")

    def _on_video_quality_warning(self, url, message):
        """Show a warning about low video quality."""
        log.warning(f"Low quality warning for {url}: {message}")
        escaped_message = html.escape(message).replace('\n', '<br>')
        msg_box = QMessageBox(self)
        msg_box.setIcon(QMessageBox.Icon.Warning)
        msg_box.setWindowTitle("Low Quality Video")
        msg_box.setTextFormat(Qt.TextFormat.RichText)
        msg_box.setText(f"The following video was downloaded at a low quality:<br><a href='{html.escape(url)}'>{html.escape(url)}</a><br><br>{escaped_message}")
        msg_box.exec()

    # -------------------------------------------------------------------------
    # DOWNLOAD LOGIC
    # -------------------------------------------------------------------------

    def start_downloads(self, urls, opts):
        """Start downloads for one or multiple URLs, handling playlist logic."""
        # Verify output directory is set before starting
        out_path = self.config_manager.get("Paths", "completed_downloads_directory", fallback="")
        if not out_path:
            QMessageBox.warning(self, "Setup Required", "Please select an output folder in Advanced Settings before downloading.")
            self.tabs.setCurrentWidget(self.tab_advanced)
            self.tab_advanced.browse_out()
            # Check again
            out_path = self.config_manager.get("Paths", "completed_downloads_directory", fallback="")
            if not out_path:
                return

        log.info(f"Starting downloads for {len(urls)} item(s) with opts: {opts}")
        self.tabs.setCurrentWidget(self.tab_active)
        self._downloads_in_progress = True

        playlist_mode = opts.get("playlist_mode", "Ask")

        for url in urls:
            is_playlist = is_likely_playlist(url)

            # Create a mutable copy of opts to modify based on user choice/logic
            url_opts = opts.copy()

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
                    url_opts["playlist_mode"] = "all"
                    # Add a placeholder while the playlist is expanded
                    self.tab_active.add_download_widget(url)
                    self.tab_active.set_placeholder_message(url, "Preparing playlist download...")
                    self._start_background_download_processing([url], url_opts, expand_playlists=True)
                elif clicked_button == btn_one:
                    # Download Single
                    url_opts["playlist_mode"] = "ignore"
                    # Do not create the UI element yet; validation/queuing will add it once ready
                    self._start_background_download_processing([url], url_opts, expand_playlists=False)
                else:
                    # Cancel
                    log.info(f"Playlist download cancelled by user for URL: {url}")

            elif is_playlist and "single" in playlist_mode.lower():
                # Download Single (ignore playlist) - already specified in settings
                url_opts["playlist_mode"] = "ignore"
                self._start_background_download_processing([url], url_opts, expand_playlists=False)

            else:
                # This branch handles:
                # 1. Not a playlist.
                # 2. A playlist with mode "Download all".
                if is_playlist:
                     url_opts["playlist_mode"] = "all"
                     # Add placeholder while playlist is prepared
                     self.tab_active.add_download_widget(url)
                     self.tab_active.set_placeholder_message(url, "Preparing playlist download...")
                
                # Only expand if it is a playlist.
                self._start_background_download_processing([url], url_opts, expand_playlists=is_playlist)


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
                            self.add_download_request.emit('__playlist_detected__', (url, len(expanded)))
                            self.add_download_request.emit('__playlist_expanded__', (url, expanded))
                        urls_to_download.extend(expanded)
                    except PlaylistExpansionError as e:
                        # Handle specific playlist errors (e.g., premiere)
                        self.download_manager.download_error.emit(url, str(e))
                        self.add_download_request.emit('__playlist_failed__', url)
                        continue  # Skip to the next URL
                    except Exception as e:
                        log.error(f"Generic error expanding playlist {url}: {e}")
                        # Fallback to trying to download the original URL
                        urls_to_download.append(url)
                else:
                    urls_to_download.append(url)

                for sub_url in urls_to_download:
                    self.add_download_request.emit(sub_url, opts)

            self.add_download_request.emit('__mark_in_progress__', None)

        thread = threading.Thread(target=_bg_worker, daemon=True)
        thread.start()

    def _handle_add_download_request(self, url, opts):
        if url.startswith('__'):
            self._handle_special_command(url, opts)
            return

        # Check archive before adding download
        if self.archive_manager.is_in_archive(url):
            reply = QMessageBox.question(
                self,
                'Download Confirmation',
                f'This URL is already in the archive:\n{url}\n\nDo you want to download it again?',
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No
            )
            if reply == QMessageBox.StandardButton.No:
                log.info(f"Skipping download of archived URL: {url}")
                # We need to inform the UI that this download is "finished" so it can be removed from active view
                # This is a bit of a hack, but it's the cleanest way to hook into the existing UI updates.
                self.download_manager.download_finished.emit(url, False) # "False" indicates it wasn't a "successful" new download
                return

        try:
            self.download_manager.add_download(url, opts)
        except Exception:
            log.exception(f"Failed to add download for {url}")

    def _handle_special_command(self, command, data):
        """Handles internal command-like signals for updating the UI from background threads."""
        if command == '__mark_in_progress__':
            try:
                self._downloads_in_progress = True
                self.tabs.setCurrentWidget(self.tab_active)
            except Exception:
                pass
        elif command == '__playlist_expanded__':
            try:
                orig, expanded = data
                self.tab_active.replace_placeholder_with_entries(orig, expanded)
            except Exception:
                log.exception("Failed to replace playlist placeholder with entries")
        elif command == '__playlist_detected__':
            try:
                orig, count = data
                self.tab_active.set_placeholder_message(orig, f"Calculating playlist ({count} items)...")
            except Exception:
                log.exception("Failed to set playlist calculating message")
        elif command == '__playlist_failed__':
            try:
                failed_url = data
                self.tab_active.remove_placeholder(failed_url)
            except Exception:
                log.exception("Failed to remove failed playlist placeholder")

    def _on_download_finished(self, url, success):
        log.info(f"Download finished: {url}, success={success}")
        if success:
            self.archive_manager.add_to_archive(url)
        else:
            self._downloads_failed.append(url)

        if self._all_downloads_complete():
            if self.exit_after:
                log.info("All downloads complete; exiting app...")
                QTimer.singleShot(1000, self.close)

    def _check_for_app_update(self):
        """Trigger the app update check via the Advanced tab helper."""
        try:
            if hasattr(self, 'tab_advanced') and hasattr(self.tab_advanced, '_check_app_update'):
                # Call the tab's check method which runs asynchronously
                self.tab_advanced._check_app_update()
        except Exception:
            log.exception('Failed to start app update check')

    def _on_download_error(self, url, message):
        log.error(f"Download failed: {url}: {message}")
        
        # Escape the message for rich text display
        escaped_message = html.escape(message).replace('\n', '<br>')
        
        msg_box = QMessageBox(self)
        msg_box.setIcon(QMessageBox.Icon.Warning)
        msg_box.setWindowTitle("Download Error")
        msg_box.setTextFormat(Qt.TextFormat.RichText)
        msg_box.setText(f"Failed to download:<br><a href='{url}'>{url}</a><br><br>{escaped_message}")
        msg_box.exec()
        
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

        # Check for leftover files in temp directory
        temp_dir = self.config_manager.get("Paths", "temporary_downloads_directory", fallback="")
        if temp_dir and os.path.isdir(temp_dir):
            try:
                # Check if directory contains any files, ignoring subdirectories
                if any(os.path.isfile(os.path.join(temp_dir, f)) for f in os.listdir(temp_dir)):
                    msg_box = QMessageBox(self)
                    msg_box.setIcon(QMessageBox.Icon.Warning)
                    msg_box.setWindowTitle("Temporary Files Found")
                    msg_box.setText("The temporary download directory is not empty. This may indicate "
                                  "incomplete or failed downloads.")
                    msg_box.setInformativeText("Do you want to open the directory to inspect the files?")
                    
                    open_button = msg_box.addButton("Open Folder", QMessageBox.ButtonRole.ActionRole)
                    exit_app_button = msg_box.addButton("Exit App", QMessageBox.ButtonRole.DestructiveRole)
                    cancel_button = msg_box.addButton(QMessageBox.StandardButton.Cancel)

                    msg_box.exec()
                    
                    clicked_button = msg_box.clickedButton()

                    if clicked_button == open_button:
                        try:
                            QDesktopServices.openUrl(QUrl.fromLocalFile(temp_dir))
                        except Exception as e:
                            log.error(f"Could not open temporary directory: {e}")
                            QMessageBox.critical(self, "Error", f"Could not open directory:\n{temp_dir}")
                        event.ignore()
                        return
                    elif clicked_button == exit_app_button:
                        pass
                    else:
                        event.ignore()
                        return
            except Exception as e:
                log.error(f"Error checking temporary directory: {e}")

        if self._downloads_failed:
            hyperlinks = [f"<a href='{url}'>{html.escape(url)}</a>" for url in self._downloads_failed]
            
            msg_box = QMessageBox(self)
            msg_box.setIcon(QMessageBox.Icon.Information)
            msg_box.setWindowTitle("Failed Downloads")
            msg_box.setTextFormat(Qt.TextFormat.RichText)
            msg_box.setText("The following downloads failed:<br><br>" + "<br>".join(hyperlinks))
            msg_box.exec()

            self.tabs.setCurrentWidget(self.tab_active)

        super().closeEvent(event)
