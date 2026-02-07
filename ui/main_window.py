import logging
import os
import html
import sys
import psutil
from PyQt6.QtWidgets import (
    QWidget, QMainWindow, QVBoxLayout, QTabWidget,
    QMessageBox, QProgressDialog, QLabel, QHBoxLayout
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QUrl
from PyQt6.QtGui import QDesktopServices

from core.config_manager import ConfigManager
from core.download_manager import DownloadManager
from core.archive_manager import ArchiveManager
from core.app_updater import AppUpdater
from core.playlist_expander import expand_playlist, is_likely_playlist, PlaylistExpansionError
from ui.tab_start import StartTab
from ui.tab_active import ActiveDownloadsTab
from ui.tab_advanced import AdvancedSettingsTab
import threading
from core import extractor_index

log = logging.getLogger(__name__)


class MediaDownloaderApp(QMainWindow):
    add_download_request = pyqtSignal(str, object)
    app_update_available = pyqtSignal(dict)
    update_progress = pyqtSignal(int)
    update_finished = pyqtSignal(bool, str)

    def __init__(self, config_manager: ConfigManager, initial_yt_dlp_version: str):
        super().__init__()
        log.info("Initializing MediaDownloaderApp")

        self.setWindowTitle("Media Downloader")
        self.resize(900, 700)

        # Core components
        self.config_manager = config_manager
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
        self.tab_advanced = AdvancedSettingsTab(self, initial_yt_dlp_version=initial_yt_dlp_version)

        self.tabs.addTab(self.tab_start, "Start a Download")
        self.tabs.addTab(self.tab_active, "Active Downloads")
        self.tabs.addTab(self.tab_advanced, "Advanced Settings")

        main_widget = QWidget()
        layout = QVBoxLayout(main_widget)
        layout.addWidget(self.tabs)

        # Bottom status row: contact + speed indicator
        self.speed_label = QLabel("Current Speed: 0.00 MB/s")
        self.speed_label.setAlignment(Qt.AlignmentFlag.AlignRight)
        self.speed_label.setStyleSheet("padding: 5px; color: #666;")

        self.contact_label = QLabel("<a href='contact'>Contact Developer</a>")
        self.contact_label.setToolTip("Open your email client to contact the developer")
        self.contact_label.setTextFormat(Qt.TextFormat.RichText)
        self.contact_label.setTextInteractionFlags(Qt.TextInteractionFlag.TextBrowserInteraction)
        self.contact_label.setOpenExternalLinks(False)
        self.contact_label.linkActivated.connect(self._on_contact_link)
        self.contact_label.setStyleSheet("padding: 5px; color: #0066cc;")

        bottom_row = QHBoxLayout()
        bottom_row.addWidget(self.contact_label, alignment=Qt.AlignmentFlag.AlignLeft)
        bottom_row.addStretch(1)
        bottom_row.addWidget(self.speed_label, alignment=Qt.AlignmentFlag.AlignRight)
        layout.addLayout(bottom_row)
        
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
        self.app_update_available.connect(self._on_app_update_available)
        self.update_progress.connect(self._on_update_progress)
        self.update_finished.connect(self._on_update_finished)

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
            
        # Timer for updating total speed
        self.speed_timer = QTimer(self)
        self.speed_timer.timeout.connect(self._update_total_speed)
        self.speed_timer.start(1000)  # Update every second
        
        # Initialize previous process IO stats for speed calculation
        try:
            self._process = psutil.Process()
            self._last_io_counters = self._get_total_io_counters()
        except Exception:
            self._process = None
            self._last_io_counters = None

    def _get_total_io_counters(self):
        """Get total IO counters for the main process and all its children."""
        if not self._process:
            return None
        
        read_bytes = 0
        
        try:
            # Include the main process
            main_io = self._process.io_counters()
            read_bytes += main_io.read_bytes
            
            # Include all child processes
            children = self._process.children(recursive=True)
            for child in children:
                try:
                    child_io = child.io_counters()
                    read_bytes += child_io.read_bytes
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    # Child process might have terminated or access is denied
                    continue
        except psutil.NoSuchProcess:
            # Main process might have terminated
            return None
            
        return read_bytes

    def _update_total_speed(self):
        """Calculate and display total download speed including child processes."""
        try:
            if not self._process:
                self._process = psutil.Process()
                self._last_io_counters = self._get_total_io_counters()
                return

            current_read_bytes = self._get_total_io_counters()
            
            if self._last_io_counters is not None and current_read_bytes is not None:
                bytes_read_since_last = current_read_bytes - self._last_io_counters
            else:
                bytes_read_since_last = 0
            
            self._last_io_counters = current_read_bytes
            
            speed_mb = bytes_read_since_last / (1024 * 1024)
            
            active_downloads = [w for w in self.download_manager.active_downloads if w.isRunning()]
            if active_downloads:
                speed_mb = max(0.0, speed_mb)
                self.speed_label.setText(f"Current Speed: {speed_mb:.2f} MB/s")
            else:
                self.speed_label.setText("Current Speed: 0.00 MB/s")
                
        except Exception as e:
            log.error(f"Error updating speed: {e}")
            self.speed_label.setText("Current Speed: -- MB/s")

    def _on_contact_link(self, _link):
        """Open the user's mail client without storing a raw email string in the codebase."""
        try:
            parts = ["vincent", "wetzel3", "gmail", "com"]
            email = f"{parts[0]}{parts[1]}@{parts[2]}.{parts[3]}"
            log.info("Opening contact email link")
            QDesktopServices.openUrl(QUrl(f"mailto:{email}"))
        except Exception:
            log.exception("Failed to open contact email link")

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
        """Check for application updates in a background thread."""
        def _check():
            try:
                updater = AppUpdater()
                update_info = updater.check_for_updates()
                if update_info:
                    self.app_update_available.emit(update_info)
            except Exception:
                log.exception("Failed to check for app updates")

        threading.Thread(target=_check, daemon=True).start()

    def _on_app_update_available(self, info):
        """Handle signal when an update is found."""
        version = info.get("version", "Unknown")
        url = info.get("url", "")
        body = info.get("body", "")
        assets = info.get("assets", [])

        msg = QMessageBox(self)
        msg.setWindowTitle("Update Available")
        msg.setText(f"A new version of MediaDownloader is available: <b>{version}</b>")
        msg.setInformativeText("Do you want to download and install it now?")
        if body:
            # Truncate body if too long
            if len(body) > 500:
                body = body[:500] + "..."
            msg.setDetailedText(body)

        update_btn = msg.addButton("Update Now", QMessageBox.ButtonRole.AcceptRole)
        view_btn = msg.addButton("View Release", QMessageBox.ButtonRole.ActionRole)
        msg.addButton("Ignore", QMessageBox.ButtonRole.RejectRole)
        msg.setDefaultButton(update_btn)

        msg.exec()

        if msg.clickedButton() == view_btn and url:
            QDesktopServices.openUrl(QUrl(url))
        elif msg.clickedButton() == update_btn:
            # Find the first asset that looks like an executable or installer
            download_url = None
            for asset in assets:
                name = asset.get("name", "").lower()
                if name.endswith(".exe") or name.endswith(".msi"):
                    download_url = asset.get("browser_download_url")
                    break
            
            if download_url:
                self._start_update_download(download_url)
            else:
                QMessageBox.warning(self, "Update Error", "No suitable update file found in the release.")
                QDesktopServices.openUrl(QUrl(url))

    def _start_update_download(self, url):
        """Starts the update download process."""
        self.update_dialog = QProgressDialog("Downloading update...", "Cancel", 0, 100, self)
        self.update_dialog.setWindowModality(Qt.WindowModality.WindowModal)
        self.update_dialog.show()

        temp_dir = self.config_manager.get("Paths", "temporary_downloads_directory", fallback=os.getcwd())
        target_path = os.path.join(temp_dir, "MediaDownloader_Update.exe")

        def _download_worker():
            updater = AppUpdater()
            success = updater.download_update(
                url, 
                target_path, 
                progress_callback=lambda p: self.update_progress.emit(p)
            )
            self.update_finished.emit(success, target_path)

        threading.Thread(target=_download_worker, daemon=True).start()

    def _on_update_progress(self, value):
        if hasattr(self, 'update_dialog'):
            self.update_dialog.setValue(value)

    def _on_update_finished(self, success, path):
        if hasattr(self, 'update_dialog'):
            self.update_dialog.close()

        if success:
            reply = QMessageBox.question(
                self, 
                "Update Ready", 
                "The update has been downloaded. The application will now restart to apply the update.",
                QMessageBox.StandardButton.Ok | QMessageBox.StandardButton.Cancel
            )
            
            if reply == QMessageBox.StandardButton.Ok:
                updater = AppUpdater()
                updater.apply_update(path)
        else:
            QMessageBox.critical(self, "Update Failed", "Failed to download the update.")

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
