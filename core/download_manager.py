import logging
import os
import sys
import shutil
import time
import json
import subprocess
from core.yt_dlp_worker import DownloadWorker, check_yt_dlp_available, fetch_metadata, is_url_valid, is_gallery_url_valid
from core.playlist_expander import expand_playlist
import threading
from core.config_manager import ConfigManager
from PyQt6.QtCore import QObject, pyqtSignal
from core.archive_manager import ArchiveManager
from core.sorting_manager import SortingManager
from core.binary_manager import get_binary_path
from urllib.parse import urlparse
import re

log = logging.getLogger(__name__)


class DownloadManager(QObject):
    download_added = pyqtSignal(object)
    download_finished = pyqtSignal(str, bool) # Reverted signal signature
    download_error = pyqtSignal(str, str)
    duplicate_download_detected = pyqtSignal(str, object)
    video_quality_warning = pyqtSignal(str, str)

    def __init__(self, config_manager):
        super().__init__()
        self.active_downloads = []
        # Queue for downloads waiting to start due to concurrency limits
        self._pending_queue = []
        # Map url -> original opts passed when starting the download
        self._original_opts = {}
        # Map url -> final destination path
        self._completed_paths = {}
        # Runtime-only override for max concurrent downloads.
        # This is not persisted and resets when the app restarts.
        self._runtime_max_threads = None
        self.config = config_manager
        self.archive_manager = ArchiveManager(self.config)
        self.sorting_manager = SortingManager(self.config)
        self.lock = threading.Lock()
        # Check yt-dlp availability on initialization
        is_available, status_msg = check_yt_dlp_available()
        log.info(f"DownloadManager initialized. yt-dlp check: {status_msg}")
        if not is_available:
            log.warning(f"yt-dlp may not be available: {status_msg}")

    def get_final_path(self, url):
        """Retrieve the final destination path for a completed download."""
        return self._completed_paths.get(url)

    def set_runtime_max_threads(self, value):
        """Set runtime-only concurrency override (1-8)."""
        try:
            parsed = int(value)
            if parsed < 1:
                parsed = 1
            if parsed > 8:
                parsed = 8
            self._runtime_max_threads = parsed
        except Exception:
            self._runtime_max_threads = None

    def _get_effective_max_threads(self):
        """Return runtime override if set; otherwise read persisted config."""
        if isinstance(self._runtime_max_threads, int):
            return self._runtime_max_threads
        try:
            return int(self.config.get("General", "max_threads", fallback="2"))
        except Exception:
            return 2

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
        max_threads = self._get_effective_max_threads()

        with self.lock:
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
                # Respect playlist mode to avoid hanging on large playlists
                playlist_mode = wrk.opts.get("playlist_mode", "Ask")
                noplaylist = False
                if "ignore" in playlist_mode.lower() or "single" in playlist_mode.lower():
                    noplaylist = True
                    
                info = fetch_metadata(wrk.url, noplaylist=noplaylist)
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
        allow_redownload = bool((opts or {}).get("allow_redownload"))
        if not allow_redownload and self.archive_manager.is_in_archive(url):
            log.info(f"Archive hit for URL: {url}. Requesting user confirmation.")
            self.duplicate_download_detected.emit(url, dict(opts or {}))
            return

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
                    # Respect playlist mode to avoid hanging on large playlists
                    playlist_mode = o.get("playlist_mode", "Ask")
                    noplaylist = False
                    if "ignore" in playlist_mode.lower() or "single" in playlist_mode.lower():
                        noplaylist = True
                        
                    if is_url_valid(u, timeout=20, noplaylist=noplaylist):
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
        workers_to_start = []
        with self.lock:
            max_threads = self._get_effective_max_threads()

            # Count currently running workers and log current state for debugging
            running = sum(1 for w in self.active_downloads if w.isRunning())
            log.debug(f"_maybe_start_next: running={running}, max_threads={max_threads}, pending={len(self._pending_queue)}")
            while running < max_threads and self._pending_queue:
                next_worker = self._pending_queue.pop(0)
                self.active_downloads.append(next_worker)
                workers_to_start.append(next_worker)
                running += 1
        
        for next_worker in workers_to_start:
            try:
                next_worker.start()
                log.debug(f"Started queued download: {next_worker.url}")
            except Exception:
                log.exception(f"Failed to start queued download: {next_worker.url}")

    def _extract_metadata_from_file(self, file_path):
        """Extract metadata from a media file using ffprobe."""
        try:
            ffprobe_path = get_binary_path("ffprobe")
            if not ffprobe_path:
                return {}

            cmd = [
                ffprobe_path,
                "-v", "quiet",
                "-print_format", "json",
                "-show_format",
                "-show_streams",
                file_path
            ]
            
            # On Windows, suppress console window
            creation_flags = 0
            if sys.platform == "win32" and getattr(sys, "frozen", False):
                creation_flags = subprocess.CREATE_NO_WINDOW

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                creationflags=creation_flags,
                encoding='utf-8',
                errors='replace'
            )

            if result.returncode == 0:
                data = json.loads(result.stdout)
                # Check format tags
                tags = data.get('format', {}).get('tags', {})
                
                # If not found, check stream tags (common for some containers like opus/mkv)
                if not tags:
                    for stream in data.get('streams', []):
                        stream_tags = stream.get('tags', {})
                        if stream_tags:
                            tags = stream_tags
                            break
                
                if tags:
                    log.debug(f"Extracted tags from {file_path}: {tags}")
                else:
                    log.debug(f"No tags found in {file_path}")

                return tags
        except Exception:
            log.exception(f"Failed to extract metadata from file: {file_path}")
        return {}

    def _move_files_job(self, worker, url, files):
        """This job runs in a background thread to move files without blocking the GUI."""
        final_destination_dir = None
        final_file_path = None
        moved_file_stems = set()
        try:
            # Prefer configured paths, but fall back to worker attributes if config is empty.
            default_completed_dir = self.config.get("Paths", "completed_downloads_directory", fallback="") or getattr(worker, "completed_dir", "")
            temp_dir = self.config.get("Paths", "temporary_downloads_directory", fallback="") or getattr(worker, "temp_dir", "")
            keep_subtitle_sidecars = (
                self.config.get("General", "subtitles_write", fallback="False") == "True"
                or self.config.get("General", "subtitles_write_auto", fallback="False") == "True"
            )
            
            if not files or not default_completed_dir:
                log.debug("No files to move or no completed_dir configured; skipping move job")
                # Even if no move, signal finished
                self.download_finished.emit(url, True)
                return

            # Normalize and ensure existence of default_completed_dir; temp_dir may be optional
            default_completed_dir = os.path.abspath(os.path.normpath(os.path.expanduser(default_completed_dir)))
            temp_dir = os.path.abspath(os.path.normpath(os.path.expanduser(temp_dir))) if temp_dir else None
            try:
                os.makedirs(default_completed_dir, exist_ok=True)
            except Exception:
                log.exception(f"Could not create default_completed_dir: {default_completed_dir}")

            log.debug(f"_move_files_job start: default_completed_dir={default_completed_dir}, temp_dir={temp_dir}, files={files}")
            # Small pause to allow worker-side postprocessing (thumbnail embedding, replace/remove)
            try:
                time.sleep(0.25)
                log.debug("Brief pause before moving files to allow worker cleanup.")
            except Exception:
                pass

            # Determine target directory based on sorting rules
            # We check metadata from the worker if available
            target_dir = default_completed_dir
            try:
                info = getattr(worker, '_meta_info', {})
                
                # Try to read the .info.json file if it exists, as it contains the most accurate metadata
                # especially after the download is complete (yt-dlp writes it)
                json_file = None
                for f in files:
                    base, ext = os.path.splitext(f)
                    candidate_json = base + ".info.json"
                    if os.path.exists(candidate_json):
                        json_file = candidate_json
                        break
                
                if json_file:
                    try:
                        with open(json_file, 'r', encoding='utf-8') as jf:
                            file_info = json.load(jf)
                            if file_info:
                                info = file_info
                                log.debug(f"Loaded metadata from JSON file: {json_file}")
                    except Exception:
                        log.warning(f"Failed to read metadata from JSON file: {json_file}")

                if info:
                    log.debug(f"Sorting metadata keys: {list(info.keys())}")
                    if 'uploader' in info:
                        log.debug(f"Sorting metadata uploader: '{info.get('uploader')}'")
                    if 'channel' in info:
                        log.debug(f"Sorting metadata channel: '{info.get('channel')}'")
                    
                    # Log album value for debugging
                    log.debug(f"Metadata album value: '{info.get('album')}'")

                # Ensure playlist_title is in info if available in opts
                opts = self._original_opts.get(url, {})
                if opts.get("playlist_title") and not info.get("playlist_title"):
                    info["playlist_title"] = opts.get("playlist_title")

                # If album is missing, try to extract it from the downloaded file(s)
                if not info.get('album'):
                    for f in files:
                        if os.path.exists(f):
                            file_tags = self._extract_metadata_from_file(f)
                            if file_tags:
                                # Merge tags into info, prioritizing existing info but filling gaps
                                # specifically looking for album
                                album = file_tags.get('album') or file_tags.get('ALBUM')
                                if album:
                                    info['album'] = album
                                    log.info(f"Extracted album from file metadata: {album}")
                                    break
                
                # If album is still missing, try to use playlist_title as fallback
                if not info.get('album'):
                    # Check if playlist_title was passed in opts
                    playlist_title = opts.get("playlist_title")
                    if playlist_title:
                        info['album'] = playlist_title
                        log.info(f"Using playlist_title as album fallback: {playlist_title}")
                    elif info.get('playlist_title'):
                         # It might be in info already if yt-dlp put it there
                         info['album'] = info.get('playlist_title')
                         log.info(f"Using info['playlist_title'] as album fallback: {info.get('playlist_title')}")

                # Reload rules to ensure we have the latest
                self.sorting_manager.load_rules()
                
                # Determine current download type
                current_download_type = "Video" # Default
                is_playlist_context = False # Default
                try:
                    opts = self._original_opts.get(url, {})
                    if opts.get("metadata_only", False):
                        current_download_type = "Metadata"
                    elif opts.get("audio_only", False):
                        current_download_type = "Audio"
                    elif opts.get("use_gallery_dl", False):
                        current_download_type = "Gallery"
                    
                    if opts.get("is_playlist_download", False):
                        is_playlist_context = True
                except Exception:
                    pass
                
                sorted_path = self.sorting_manager.get_target_path(info, current_download_type=current_download_type, is_playlist_context=is_playlist_context)
                if sorted_path:
                    # Ensure the sorted path exists
                    sorted_path = os.path.abspath(os.path.normpath(os.path.expanduser(sorted_path)))
                    try:
                        os.makedirs(sorted_path, exist_ok=True)
                        target_dir = sorted_path
                        log.info(f"Sorting rule matched. Target directory changed to: {target_dir}")
                    except Exception:
                        log.exception(f"Could not create sorted target directory: {sorted_path}. Falling back to default.")
            except Exception:
                log.exception("Error applying sorting rules")

            final_destination_dir = target_dir

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
                    # relative to default_completed_dir (in case it was somehow put there already)
                    if default_completed_dir:
                        candidates.append(os.path.join(default_completed_dir, src))
                        candidates.append(os.path.join(default_completed_dir, os.path.basename(src)))
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
                            candidate = _find_candidate_in_dir(temp_dir) or _find_candidate_in_dir(default_completed_dir) or _find_candidate_in_dir(os.getcwd())

                        if candidate:
                            file_to_move = candidate
                            log.info(f"Recovery found candidate file: {candidate}")

                    if file_to_move:
                        try:
                            abs_path = os.path.abspath(file_to_move)
                            dst = os.path.normpath(os.path.join(target_dir, os.path.basename(abs_path)))

                            if os.path.abspath(dst) == os.path.abspath(abs_path):
                                log.info(f"File already in target directory, skipping move: {abs_path}")
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
                                    moved_file_stems.add(os.path.splitext(os.path.basename(abs_path))[0])
                                    if final_file_path is None:
                                        final_file_path = dst
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
                            
                            # Clean up the .info.json file
                            try:
                                json_src = os.path.splitext(abs_path)[0] + ".info.json"
                                if os.path.exists(json_src):
                                    os.remove(json_src)
                                    log.info(f"Removed metadata file {json_src}")
                            except Exception:
                                log.warning(f"Failed to remove metadata file for {abs_path}")

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

            # When sidecar subtitles are not requested, remove any subtitle artifacts
            # that may have been downloaded for embedding and left in temp.
            if temp_dir and os.path.isdir(temp_dir) and not keep_subtitle_sidecars and moved_file_stems:
                subtitle_exts = {
                    ".srt", ".vtt", ".ass", ".ssa", ".lrc", ".ttml", ".json3", ".srv1", ".srv2", ".srv3"
                }
                temp_dir_abs = os.path.abspath(temp_dir)
                try:
                    for root, _, temp_files in os.walk(temp_dir_abs):
                        for name in temp_files:
                            lower_name = name.lower()
                            stem, ext = os.path.splitext(lower_name)
                            if ext not in subtitle_exts:
                                continue
                            for moved_stem in moved_file_stems:
                                moved_stem_lower = moved_stem.lower()
                                # Supports subtitle names like:
                                # "<stem>.srt" and "<stem>.en.srt"
                                if stem == moved_stem_lower or stem.startswith(moved_stem_lower + "."):
                                    candidate = os.path.abspath(os.path.join(root, name))
                                    if os.path.commonpath([candidate, temp_dir_abs]) == temp_dir_abs:
                                        try:
                                            os.remove(candidate)
                                            log.info(f"Removed leftover subtitle temp file: {candidate}")
                                        except Exception as remove_err:
                                            log.debug(f"Could not remove leftover subtitle temp file {candidate}: {remove_err}")
                                    break
                except Exception:
                    log.debug("Subtitle temp cleanup encountered an error", exc_info=True)

        except Exception:
            log.exception("Error in file moving job")
        
        # Store final path
        if final_file_path:
            self._completed_paths[url] = str(final_file_path)
        elif final_destination_dir:
            self._completed_paths[url] = str(final_destination_dir)

        # Emit finished signal (reverted to 2 args)
        try:
            log.debug(f"Emitting download_finished for {url}")
            self.download_finished.emit(url, True)
        except Exception:
            log.exception(f"Failed to emit download_finished signal for {url}")


    def _on_worker_finished(self, worker, url, success, files=None):
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
            # If no files to move, still emit finished signal (e.g. for failed downloads or no-file success)
            # But wait, if success is False, we emit finished(False) below.
            # If success is True but no files, we should probably emit finished(True) here.
            if success:
                 self.download_finished.emit(url, True)

        # Clean up active downloads list (remove this worker if present)
        with self.lock:
            try:
                if worker in self.active_downloads:
                    self.active_downloads.remove(worker)
            except Exception:
                pass

        if not success:
             self.download_finished.emit(url, False)

        # Start next queued download if any
        self._maybe_start_next()
        # Debug: summarize completed directory contents after processing.
        # Avoid dumping huge system-folder listings (e.g. $RECYCLE.BIN on drive roots).
        try:
            completed_dir = self.config.get("Paths", "completed_downloads_directory", fallback="")
            if completed_dir and os.path.exists(completed_dir):
                try:
                    ignored_dirs = {"$RECYCLE.BIN", "System Volume Information"}
                    file_count = 0
                    sample = []
                    for root, dirnames, files_in_dir in os.walk(completed_dir):
                        # Skip Windows system directories when output points to a drive root.
                        dirnames[:] = [d for d in dirnames if d not in ignored_dirs]
                        for fn in files_in_dir:
                            p = os.path.join(root, fn)
                            file_count += 1
                            if len(sample) < 10:
                                sample.append(os.path.relpath(p, completed_dir))
                    log.debug(
                        f"Completed dir summary ({completed_dir}): files={file_count}, sample={sample}"
                    )
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
        
        # Run the handler in a background thread to avoid blocking the UI with
        # archive I/O or other synchronous post-processing.
        finish_thread = threading.Thread(
            target=self._on_worker_finished,
            args=(worker, url, success, files),
            daemon=True
        )
        finish_thread.start()

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
        with self.lock:
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
