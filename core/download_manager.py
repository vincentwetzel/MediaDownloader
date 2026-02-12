import logging
import os
import sys
import shutil
import time
from core.yt_dlp_worker import DownloadWorker, check_yt_dlp_available, fetch_metadata, is_url_valid, is_gallery_url_valid
from core.playlist_expander import expand_playlist
import threading
from core.config_manager import ConfigManager
from PyQt6.QtCore import QObject, pyqtSignal
from core.archive_manager import ArchiveManager
from core.binary_manager import get_binary_path
from urllib.parse import urlparse
import re

log = logging.getLogger(__name__)


class DownloadManager(QObject):
    download_added = pyqtSignal(object)
    download_finished = pyqtSignal(str, bool)
    download_error = pyqtSignal(str, str)
    video_quality_warning = pyqtSignal(str, str)

    def __init__(self):
        super().__init__()
        self.active_downloads = []
        # Queue for downloads waiting to start due to concurrency limits
        self._pending_queue = []
        # Map url -> original opts passed when starting the download
        self._original_opts = {}
        self.config = ConfigManager()
        self.archive_manager = ArchiveManager()
        # Check yt-dlp availability on initialization
        is_available, status_msg = check_yt_dlp_available()
        log.info(f"DownloadManager initialized. yt-dlp check: {status_msg}")
        if not is_available:
            log.warning(f"yt-dlp may not be available: {status_msg}")

    def _enqueue_single_download(self, url, opts, parent_url=None):
        """Create and queue a single DownloadWorker for the given URL.
        Keeps the original behavior but is factored so playlists can be expanded
        into multiple individual enqueues.
        """
        # Store original opts so retry/resume can reuse them
        try:
            self._original_opts[url] = opts
        except Exception:
            self._original_opts[url] = opts

        # We now pass the opts dictionary directly to the worker.
        # The worker will handle argument construction.
        log.info(f"Queueing download for {url} with opts: {opts}")
        worker = DownloadWorker(url, opts, self.config)
        worker.progress.connect(self._on_progress)
        # Connect finished/error signals to wrapper slots that use QObject.sender()
        # This avoids potential issues with lambdas and ensures the original worker
        # object is available via sender() when the slot runs.
        worker.finished.connect(self._on_worker_finished_signal)
        worker.error.connect(self._on_error_signal)

        # Attach temp/completed dir info to worker for post-processing
        worker.temp_dir = self.config.get("Paths", "temporary_downloads_directory", fallback="")
        worker.completed_dir = self.config.get("Paths", "completed_downloads_directory", fallback="")

        # Decide whether to start immediately or queue based on current concurrency
        try:
            max_threads = int(self.config.get("General", "max_threads", fallback="2"))
        except Exception:
            max_threads = 2

        running = sum(1 for w in self.active_downloads if w.isRunning())
        if running < max_threads:
            # Start immediately
            self.active_downloads.append(worker)
            try:
                worker.start()
                log.debug(f"Started download immediately: {worker.url}")
            except Exception:
                log.exception(f"Failed to start download immediately: {worker.url}")
        else:
            # Add to pending queue and notify UI
            self._pending_queue.append(worker)
            log.debug(f"Download queued: {url}")

        # Emit for UI to create per-download UI element
        self.download_added.emit(worker)

        # Fetch metadata (title) in background so queued items can show their title
        def _fetch_and_emit_title(wrk):
            try:
                info = fetch_metadata(wrk.url)
                if info:
                    title = info.get("title") or info.get("id")
                    try:
                        wrk._meta_info = info
                    except Exception:
                        pass
                    if title:
                        try:
                            wrk.title_updated.emit(str(title))
                        except Exception:
                            pass
            except Exception:
                pass

        try:
            t = threading.Thread(target=_fetch_and_emit_title, args=(worker,), daemon=True)
            t.start()
        except Exception:
            pass

        # Attempt to start queued downloads in case slots are available
        self._maybe_start_next()

    def add_download(self, url, opts):
        """Public entry point for adding a single download request.

        Playlist expansion is handled by the UI/background worker so this
        method keeps enqueuing lightweight and non-blocking.
        """
        # Tier 1: Fast-Track for YouTube
        # Immediate string/regex check for high-traffic domains (e.g., youtube.com, youtu.be, music.youtube.com).
        # These are accepted instantly to provide zero-latency UI feedback.
        yt_pattern = r'^(?:https?://)?(?:www\.|music\.)?(?:youtube\.com|youtu\.be)/.+$'
        if re.match(yt_pattern, url, re.IGNORECASE):
            log.info(f"Tier 1 validation passed (YouTube Fast-Track): {url}")
            self._enqueue_single_download(url, opts)
            return

        # Consult the extractor index (if available). If the host is recognized by an
        # extractor, proceed immediately.
        try:
            parsed = urlparse(url)
            host = (parsed.hostname or "").lower()
            from core.extractor_index import host_supported
            try:
                if host and host_supported(host):
                    log.info(f"Tier 1 validation passed (Extractor Index): {url}")
                    self._enqueue_single_download(url, opts)
                    return
            except Exception:
                # silently ignore index failures and continue to validation fallback
                pass
        except Exception:
            # index module not available; proceed to validation fallback
            pass

        # Tier 2: Metadata Probe (Simulate)
        # For less-common domains, the app initiates a background `yt-dlp --simulate` call.
        # The "Active Download" UI entry is only generated if this probe confirms the URL is a valid target.
        def _validate_and_enqueue(u, o):
            try:
                # Check if gallery mode is enabled
                use_gallery_dl = o.get("use_gallery_dl", False)
                
                if use_gallery_dl:
                    # Use gallery-dl validation
                    if is_gallery_url_valid(u, timeout=20):
                        self._enqueue_single_download(u, o)
                    else:
                        # Fallback for common gallery sites
                        common_gallery_hosts = ['instagram.com', 'twitter.com', 'x.com', 'reddit.com', 'tumblr.com', 'deviantart.com', 'artstation.com', 'pixiv.net']
                        parsed = urlparse(u)
                        host = (parsed.hostname or "").lower()
                        if any(ch in host for ch in common_gallery_hosts):
                            log.info(f"Tier 2 validation failed for {u} but host is common gallery site. Enqueuing anyway.")
                            self._enqueue_single_download(u, o)
                        else:
                            log.info(f"Tier 2 validation failed for {u} (gallery-dl simulate)")
                            try:
                                self.download_error.emit(u, "Unsupported or invalid URL for Gallery Download")
                            except Exception:
                                pass
                else:
                    # Use yt-dlp validation
                    if is_url_valid(u, timeout=20):
                        self._enqueue_single_download(u, o)
                    else:
                        log.info(f"Tier 2 validation failed for {u} (yt-dlp simulate)")
                        try:
                            self.download_error.emit(u, "Unsupported or invalid URL")
                        except Exception:
                            pass
            except Exception:
                log.exception(f"Exception during Tier 2 validation for: {u}")

        try:
            t = threading.Thread(target=_validate_and_enqueue, args=(url, opts), daemon=True)
            t.start()
        except Exception:
            # Fallback: if background validation cannot be started, enqueue directly
            try:
                self._enqueue_single_download(url, opts)
            except Exception:
                log.exception(f"Failed to enqueue download: {url}")

    def _on_progress(self, data):
        log.debug(f"Progress: {data}")

    def _maybe_start_next(self):
        """Start downloads from the pending queue up to configured concurrency."""
        try:
            max_threads = int(self.config.get("General", "max_threads", fallback="2"))
        except Exception:
            max_threads = 2

        # Count currently running workers and log current state for debugging
        running = sum(1 for w in self.active_downloads if w.isRunning())
        log.debug(f"_maybe_start_next: running={running}, max_threads={max_threads}, pending={len(self._pending_queue)}")
        while running < max_threads and self._pending_queue:
            next_worker = self._pending_queue.pop(0)
            self.active_downloads.append(next_worker)
            try:
                next_worker.start()
                log.debug(f"Started queued download: {next_worker.url}")
            except Exception:
                log.exception(f"Failed to start queued download: {next_worker.url}")
            running += 1


    def _move_files_job(self, worker, url, files):
        """This job runs in a background thread to move files without blocking the GUI."""
        try:
            # Prefer configured paths, but fall back to worker attributes if config is empty.
            completed_dir = self.config.get("Paths", "completed_downloads_directory", fallback="") or getattr(worker, "completed_dir", "")
            temp_dir = self.config.get("Paths", "temporary_downloads_directory", fallback="") or getattr(worker, "temp_dir", "")
            if not files or not completed_dir:
                log.debug("No files to move or no completed_dir configured; skipping move job")
                return

            # Normalize and ensure existence of completed_dir; temp_dir may be optional
            completed_dir = os.path.abspath(os.path.normpath(os.path.expanduser(completed_dir)))
            temp_dir = os.path.abspath(os.path.normpath(os.path.expanduser(temp_dir))) if temp_dir else None
            try:
                os.makedirs(completed_dir, exist_ok=True)
            except Exception:
                log.exception(f"Could not create completed_dir: {completed_dir}")

            log.debug(f"_move_files_job start: completed_dir={completed_dir}, temp_dir={temp_dir}, files={files}")
            # Small pause to allow worker-side postprocessing (thumbnail embedding, replace/remove)
            try:
                time.sleep(0.25)
                log.debug("Brief pause before moving files to allow worker cleanup.")
            except Exception:
                pass

            for f in files:
                try:
                    raw = str(f).strip().strip('"\'')
                    src = os.path.normpath(raw)
                    # Skip intermediate files
                    if src.lower().endswith(('.part', '.ytdl')):
                        log.debug(f"Skipping intermediate file: {src}")
                        continue

                    # Build candidate paths to try before doing a directory search
                    candidates = []
                    # as-is
                    candidates.append(src)
                    # relative to temp_dir
                    if temp_dir:
                        candidates.append(os.path.join(temp_dir, src))
                        candidates.append(os.path.join(temp_dir, os.path.basename(src)))
                    # relative to completed_dir
                    if completed_dir:
                        candidates.append(os.path.join(completed_dir, src))
                        candidates.append(os.path.join(completed_dir, os.path.basename(src)))
                    # relative to cwd
                    candidates.append(os.path.abspath(src))
                    candidates.append(os.path.join(os.getcwd(), src))

                    # Log candidate list and existence checks for diagnostics
                    try:
                        log.debug(f"Move candidates for reported src '{src}': {candidates}")
                        file_to_move = None
                        for cand in list(candidates):
                            try:
                                cand_norm = os.path.normpath(cand)
                                exists = os.path.exists(cand_norm)
                                log.debug(f"Candidate check: {cand_norm} exists={exists}")
                                if exists:
                                    file_to_move = cand_norm
                                    log.debug(f"Found candidate for move: {file_to_move} (from {cand})")
                                    break
                            except Exception:
                                log.debug(f"Exception while checking candidate: {cand}", exc_info=True)
                                continue
                    except Exception:
                        log.debug("Error while building/checking move candidates", exc_info=True)

                    if not file_to_move:
                        log.warning(f"Reported source missing, attempting recovery search for: {src}")
                        # Recovery logic using tokens
                        candidate = None
                        search_tokens = []
                        try:
                            info = getattr(worker, '_meta_info', {})
                            vid = info.get('id')
                            title = info.get('title')
                            if vid:
                                search_tokens.append(str(vid))
                            if title:
                                search_tokens.append(title[:60])
                        except Exception:
                            pass

                        def _find_candidate_in_dir(d):
                            if not d or not os.path.isdir(d):
                                return None
                            for root, _, files_list in os.walk(d):
                                for name in files_list:
                                    for t in search_tokens:
                                        if t and t.lower() in name.lower():
                                            return os.path.join(root, name)
                            return None

                        if search_tokens:
                            candidate = _find_candidate_in_dir(temp_dir) or _find_candidate_in_dir(completed_dir) or _find_candidate_in_dir(os.getcwd())

                        if candidate:
                            file_to_move = candidate
                            log.info(f"Recovery found candidate file: {candidate}")

                    if file_to_move:
                        try:
                            abs_path = os.path.abspath(file_to_move)
                            dst = os.path.normpath(os.path.join(completed_dir, os.path.basename(abs_path)))

                            if os.path.abspath(dst) == os.path.abspath(abs_path):
                                log.info(f"File already in completed_dir, skipping move: {abs_path}")
                                continue

                            if not self._wait_for_file_stable(abs_path, timeout=10):
                                log.warning(f"File {abs_path} did not stabilize, proceeding with move anyway.")

                            for attempt in range(5):
                                try:
                                    if os.path.exists(dst) and os.path.abspath(dst) != abs_path:
                                        try:
                                            os.remove(dst)
                                        except Exception:
                                            log.debug(f"Could not remove existing destination {dst}, will attempt move anyway")
                                    shutil.move(abs_path, dst)
                                    log.info(f"Moved file {abs_path} -> {dst}")
                                    break
                                except (FileNotFoundError, PermissionError, OSError) as e:
                                    log.exception(f"Attempt {attempt+1}: Error moving {abs_path} to {dst}")
                                    if isinstance(e, FileNotFoundError):
                                        log.debug(f"Source file disappeared before move: {abs_path}")
                                        break
                                    # Retry on sharing violation (Windows)
                                    winerr = getattr(e, 'winerror', 0)
                                    if winerr == 32 or 'being used by another process' in str(e).lower():
                                        time.sleep(0.2 * (attempt + 1))
                                        continue
                                    break
                        except Exception:
                            log.exception(f"Failed to prepare or execute move for {file_to_move}")
                    else:
                        log.error(f"Could not find or recover source file for moving: {src}")

                except Exception as e:
                    log.debug(f"Skipping file move for {f}: {e}")

            # After moving, clean up leftover .part files
            for f in files:
                if str(f).lower().endswith('.part'):
                    try:
                        p = os.path.normpath(str(f).strip().strip('\"\''))
                        if temp_dir and os.path.exists(p) and os.path.commonpath([os.path.abspath(p), os.path.abspath(temp_dir)]) == os.path.abspath(temp_dir):
                            os.remove(p)
                            log.info(f"Removed leftover part file: {p}")
                    except Exception as e:
                        log.debug(f"Could not remove leftover part file {f}: {e}")

        except Exception:
            log.exception("Error in file moving job")


    def _on_worker_finished(self, worker, url, success, files=None):
        print(f"[HANDLER] _on_worker_finished called: url={url}, success={success}, files={files}")  # Direct print
        log.info(f"Download finished: {url} success={success} files={files}")
        log.debug(f"_on_worker_finished: files type={type(files)}, files value={files}, bool(files)={bool(files)}")

        # If download was successful, add to archive and check for low quality video
        if success:
            self.archive_manager.add_to_archive(url)
            try:
                opts = self._original_opts.get(url, {})
                # Check if it's a video download and "best" quality was requested
                if not opts.get("audio_only") and (opts.get("video_quality") or "best") == "best":
                    info = getattr(worker, '_meta_info', {})
                    if info:
                        height = info.get('height')
                        # Ensure height is a number and is 480 or less
                        if isinstance(height, (int, float)) and height <= 480:
                            title = info.get('title', url)
                            message = (f"The video '{title}' was downloaded, but the highest available quality "
                                       f"was {height}p, which is considered low quality.")
                            self.video_quality_warning.emit(url, message)
            except Exception:
                log.exception("Error checking for video quality warning.")

        # If files were created, move them in a background thread
        if success and files:
            log.warning(f"Move job triggered: files count={len(files) if isinstance(files, (list, tuple)) else '?'}")
            move_thread = threading.Thread(
                target=self._move_files_job,
                args=(worker, url, list(files)),  # Pass a copy of the list
            )
            # Ensure move thread is non-daemon so file moves complete before process exit
            move_thread.daemon = False
            move_thread.start()
        else:
            log.error(f"Move job NOT triggered: success={success}, files={files}, bool(files)={bool(files)}")

        # Clean up active downloads list (remove this worker if present)
        try:
            if worker in self.active_downloads:
                self.active_downloads.remove(worker)
        except Exception:
            pass

        self.download_finished.emit(url, success)

        # Start next queued download if any
        self._maybe_start_next()
        # Debug: list completed directory contents after processing to help diagnose
        try:
            completed_dir = self.config.get("Paths", "completed_downloads_directory", fallback="")
            if completed_dir and os.path.exists(completed_dir):
                try:
                    listing = []
                    for root, _, files_in_dir in os.walk(completed_dir):
                        for fn in files_in_dir:
                            p = os.path.join(root, fn)
                            try:
                                listing.append((os.path.relpath(p, completed_dir), os.path.getsize(p), os.path.getmtime(p)))
                            except Exception:
                                listing.append((os.path.relpath(p, completed_dir), None, None))
                    log.debug(f"Completed dir full listing ({completed_dir}): {listing[:200]}")
                except Exception:
                    log.debug("Could not list completed_dir for debug")
        except Exception:
            pass

    def _on_worker_finished_signal(self, url, success, files=None):
        """Wrapper slot for worker.finished signal. Uses sender() to obtain the worker."""
        try:
            worker = self.sender()
        except Exception:
            worker = None
        self._on_worker_finished(worker, url, success, files)

    def _on_error_signal(self, url, message):
        """Wrapper slot for worker.error signal. Uses sender() to obtain the worker."""
        try:
            worker = self.sender()
        except Exception:
            worker = None
        self._on_error(worker, url, message)


    def _wait_for_file_stable(self, path, timeout=10, poll_interval=0.5):
        """Wait until file size and mtime are stable for one poll interval or until timeout.
        Returns True if stable, False if timeout reached.
        """
        try:
            if not os.path.exists(path):
                return False
            end_time = __import__('time').time() + float(timeout)
            last_size = os.path.getsize(path)
            last_mtime = os.path.getmtime(path)
            while __import__('time').time() < end_time:
                __import__('time').sleep(poll_interval)
                try:
                    size = os.path.getsize(path)
                    mtime = os.path.getmtime(path)
                except Exception:
                    return False
                if size == last_size and mtime == last_mtime:
                    return True
                last_size, last_mtime = size, mtime
            return False
        except Exception:
            return False
        

    def _on_error(self, worker, url, message):
        log.error(f"Error on {url}: {message}")
        # Remove from active list if present
        try:
            if worker in self.active_downloads:
                self.active_downloads.remove(worker)
        except Exception:
            pass
        # Map common yt-dlp error messages to concise, user-friendly text
        user_message = None
        try:
            low = (message or "").lower()
            if "video unavailable" in low or "video is unavailable" in low or ("video" in low and "unavailable" in low):
                user_message = f"The video at {url} cannot be downloaded because it appears to be unavailable. It may have been removed or made private."
            elif "private" in low and "video" in low:
                user_message = f"The video at {url} cannot be downloaded because it is private. You may not have permission to view it."
            elif "not found" in low and "video" in low:
                user_message = f"The video at {url} could not be found. It may have been deleted."
            elif "sign in to confirm" in low:
                user_message = f"YouTube requires sign-in for this video. Please select a browser in Advanced Settings to use its cookies."
            elif "http error 403" in low:
                user_message = f"Access forbidden (403). This usually means YouTube is blocking the request. Try updating yt-dlp or using cookies from a browser."
            elif "n challenge solving failed" in low or "javascript runtime" in low:
                user_message = f"YouTube's anti-bot measures require a JavaScript runtime (like Deno or Node.js). Please install one and configure its path in Advanced Settings."
        except Exception:
            user_message = None

        # Fallback to original message if no mapping found
        emit_msg = user_message if user_message else message
        self.download_error.emit(url, emit_msg)

        # Start next queued download if any
        self._maybe_start_next()
