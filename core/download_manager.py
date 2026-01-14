import logging
import os
import sys
import shutil
from core.yt_dlp_worker import DownloadWorker, check_yt_dlp_available, fetch_metadata
import threading
from core.config_manager import ConfigManager
from PyQt6.QtCore import QObject, pyqtSignal

log = logging.getLogger(__name__)


class DownloadManager(QObject):
    download_added = pyqtSignal(object)
    download_finished = pyqtSignal(str, bool)
    download_error = pyqtSignal(str, str)

    def __init__(self):
        super().__init__()
        self.active_downloads = []
        # Queue for downloads waiting to start due to concurrency limits
        self._pending_queue = []
        # Map url -> original opts passed when starting the download
        self._original_opts = {}
        self.config = ConfigManager()
        # Check yt-dlp availability on initialization
        is_available, status_msg = check_yt_dlp_available()
        log.info(f"DownloadManager initialized. yt-dlp check: {status_msg}")
        if not is_available:
            log.warning(f"yt-dlp may not be available: {status_msg}")

    def _convert_opts_to_args(self, opts):
        """Convert options dict to list of command-line arguments."""
        if isinstance(opts, list):
            return opts  # Already a list
        
        args = []
        if not isinstance(opts, dict):
            return args
        
        # Output directory (write to temporary downloads directory first)
        temp_dir = self.config.get("Paths", "temporary_downloads_directory", fallback="")
        output_dir = temp_dir or self.config.get("Paths", "completed_downloads_directory", fallback="")
        if output_dir:
            # Ensure directory exists and is writable
            try:
                os.makedirs(output_dir, exist_ok=True)
                # Test if directory is writable
                test_file = os.path.join(output_dir, ".test_write")
                try:
                    with open(test_file, 'w') as f:
                        f.write("test")
                    os.remove(test_file)
                except (PermissionError, OSError) as e:
                    log.error(f"Output directory is not writable: {output_dir} - {e}")
                    # Don't set output directory if not writable
                    output_dir = ""
            except (PermissionError, OSError) as e:
                log.error(f"Could not create/access output directory {output_dir}: {e}")
                output_dir = ""
            
            if output_dir:
                # Build yt-dlp output template in the configured completed-downloads directory.
                # Use a safe filename template and convert backslashes to forward slashes
                # so yt-dlp won't misinterpret Windows backslashes.
                # Use the requested filename template: title up to 90 chars, uploader up to 30 chars,
                # upload date formatted MM-DD-YYYY, and id.
                output_template = os.path.join(output_dir, "%(title).90s [%(uploader).30s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s")
                output_template = output_template.replace("\\", "/")
                args.extend(["-o", output_template])
        
        # Ensure yt-dlp emits progress updates as separate lines so the
        # subprocess reader can capture incremental progress (avoids only
        # receiving progress at completion when yt-dlp uses carriage returns).
        args.append("--newline")
        
        # Audio only mode
        if opts.get("audio_only"):
            args.append("--extract-audio")
            audio_ext = opts.get("audio_ext") or self.config.get("General", "audio_ext", fallback="mp3") or "mp3"
            args.extend(["--audio-format", audio_ext])
            audio_quality = opts.get("audio_quality") or self.config.get("General", "audio_quality", fallback="best")
            if audio_quality and audio_quality != "best":
                # Convert "192k" to "192" for yt-dlp
                quality_val = str(audio_quality).replace("k", "").replace("K", "")
                args.extend(["--audio-quality", quality_val])
        else:
            # Video format selection
            video_quality = opts.get("video_quality") or self.config.get("General", "video_quality", fallback="best")
            if video_quality and video_quality != "best":
                # Convert quality like "1080p" to format selector
                try:
                    height = int(video_quality.replace("p", "").replace("P", ""))
                    args.extend(["-f", f"bestvideo[height<={height}]+bestaudio/best"])
                except ValueError:
                    # If parsing fails, use best
                    log.warning(f"Invalid video quality format: {video_quality}, using best")
            
            video_ext = opts.get("video_ext") or self.config.get("General", "video_ext", fallback="")
            if video_ext:
                args.extend(["--merge-output-format", video_ext])
        
        # Playlist handling
        playlist_mode = opts.get("playlist_mode", "Ask")
        if "ignore" in playlist_mode.lower() or "single" in playlist_mode.lower():
            args.append("--no-playlist")
        elif "all" in playlist_mode.lower() or "no prompt" in playlist_mode.lower():
            args.append("--yes-playlist")
        
        # SponsorBlock
        if self.config.get("General", "sponsorblock", fallback="True") == "True":
            args.append("--sponsorblock-remove")
            args.append("sponsor,intro,outro,selfpromo,interaction,preview,music_offtopic")
        
        # Restrict filenames
        # Filename sanitization: prefer explicit `windowsfilenames` config.
        # If `windowsfilenames` is True, pass `--windows-filenames` to yt-dlp
        # which specifically targets Windows-invalid characters (e.g. ':').
        # If `restrict_filenames` is True, pass `--restrict-filenames` instead.
        # If neither is set and we're on Windows, default to `--windows-filenames`.
        try:
            windows_cfg = self.config.get("General", "windowsfilenames", fallback=None)
        except Exception:
            windows_cfg = None
        try:
            restrict_cfg = self.config.get("General", "restrict_filenames", fallback=None)
        except Exception:
            restrict_cfg = None

        if str(windows_cfg) == "True":
            args.append("--windows-filenames")
        elif str(restrict_cfg) == "True":
            args.append("--restrict-filenames")
        else:
            # Default to windows-safe names on Windows if user hasn't configured
            try:
                if os.name == 'nt':
                    args.append("--windows-filenames")
                    log.debug("Defaulting to --windows-filenames on Windows")
            except Exception:
                pass
        
        # Rate limit
        rate_limit = self.config.get("General", "rate_limit", fallback="0")
        if rate_limit and rate_limit not in ("0", "", "no limit", "No limit"):
            args.extend(["--limit-rate", rate_limit])
        
        return args

    def add_download(self, url, opts):
        # Convert opts dict to command-line args if needed
        # Store original opts so retry/resume can reuse them
        try:
            self._original_opts[url] = opts
        except Exception:
            self._original_opts[url] = opts

        cmd_args = self._convert_opts_to_args(opts)
        log.info(f"Queueing download for {url} with args: {cmd_args}")
        worker = DownloadWorker(url, cmd_args)
        worker.progress.connect(self._on_progress)
        # Bind the worker object into the callbacks so we can identify and
        # remove it from active lists when it finishes/errors.
        worker.finished.connect(lambda url, success, files, w=worker: self._on_finished(w, url, success, files))
        worker.error.connect(lambda url, msg, w=worker: self._on_error(w, url, msg))

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
            # best-effort; don't block or crash if thread can't start
            pass

        # Attempt to start queued downloads in case slots are available
        self._maybe_start_next()

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


    def _on_finished(self, worker, url, success, files=None):
        log.info(f"Download finished: {url} success={success} files={files}")
        # If files were created in the temp dir, move them to the completed directory
        try:
            completed_dir = self.config.get("Paths", "completed_downloads_directory", fallback="")
            temp_dir = self.config.get("Paths", "temporary_downloads_directory", fallback="")
            if success and files and completed_dir:
                # Normalize configured directories to avoid mixed slashes
                try:
                    completed_dir = os.path.normpath(completed_dir)
                except Exception:
                    pass
                try:
                    temp_dir = os.path.normpath(temp_dir) if temp_dir else temp_dir
                except Exception:
                    pass
                os.makedirs(completed_dir, exist_ok=True)
                for f in files:
                    try:
                        # normalize and strip surrounding quotes that sometimes appear in yt-dlp output
                        src = os.path.normpath(str(f).strip().strip('"\''))
                        # Ensure local abs_* variables exist for both branches
                        abs_src = None
                        abs_completed = None
                        abs_temp = None
                        # Skip intermediate .part files (yt-dlp part files)
                        if src.lower().endswith('.part'):
                            log.debug(f"Skipping intermediate part file: {src}")
                            continue
                        if os.path.exists(src):
                            abs_src = os.path.abspath(src)
                            abs_completed = os.path.abspath(completed_dir) if completed_dir else None
                            abs_temp = os.path.abspath(temp_dir) if temp_dir else None
                        else:
                            # Source reported by yt-dlp does not exist. Try id-based
                            # recovery: list temp/completed dirs and search for files
                            # containing the media id or title as a fallback.
                            try:
                                log.warning(f"Reported source missing, attempting recovery search: {src}")
                                # Small debug listing of directories to help diagnose timing issues
                                try:
                                    if temp_dir and os.path.exists(temp_dir):
                                        entries = os.listdir(temp_dir)
                                        log.debug(f"Temp dir listing ({temp_dir}) sample: {entries[:50]}")
                                except Exception:
                                    log.debug("Could not list temp_dir contents for debug")
                                try:
                                    if completed_dir and os.path.exists(completed_dir):
                                        entries = os.listdir(completed_dir)
                                        log.debug(f"Completed dir listing ({completed_dir}) sample: {entries[:50]}")
                                except Exception:
                                    log.debug("Could not list completed_dir contents for debug")

                                # Use metadata id/title if available to find candidate files
                                candidate = None
                                search_tokens = []
                                try:
                                    info = getattr(worker, '_meta_info', None)
                                    if info:
                                        vid = info.get('id') or ''
                                        title = info.get('title') or ''
                                        if vid:
                                            search_tokens.append(str(vid))
                                        if title:
                                            # Use a shortened, file-safe part of the title
                                            search_tokens.append(title[:60])
                                except Exception:
                                    pass

                                # Helper to scan a directory for filenames containing tokens
                                def _find_candidate_in_dir(d):
                                    try:
                                        for root, _, files_list in os.walk(d):
                                            for name in files_list:
                                                lname = name.lower()
                                                for t in search_tokens:
                                                    if not t:
                                                        continue
                                                    if t.lower() in lname:
                                                        return os.path.join(root, name)
                                        return None
                                    except Exception:
                                        return None

                                if search_tokens:
                                    # Prefer temp_dir search first (likely location)
                                    if temp_dir and os.path.exists(temp_dir):
                                        candidate = _find_candidate_in_dir(temp_dir)
                                    if not candidate and completed_dir and os.path.exists(completed_dir):
                                        candidate = _find_candidate_in_dir(completed_dir)

                                recovered_moved = False
                                if candidate:
                                    try:
                                        cand_abs = os.path.abspath(candidate)
                                        abs_completed = os.path.abspath(completed_dir) if completed_dir else None
                                        dst = os.path.normpath(os.path.join(abs_completed, os.path.basename(cand_abs))) if abs_completed else None
                                        # Wait for stability
                                        try:
                                            if not self._wait_for_file_stable(cand_abs, timeout=10):
                                                log.warning(f"Candidate {cand_abs} did not stabilize before move; proceeding anyway")
                                        except Exception:
                                            pass
                                        moved = False
                                        for attempt in range(5):
                                            try:
                                                # Avoid removing the destination if it is the same
                                                # as the candidate source (can happen when the
                                                # candidate was already in the completed dir).
                                                if dst and os.path.exists(dst):
                                                    try:
                                                        if os.path.abspath(dst) != os.path.abspath(cand_abs):
                                                            os.remove(dst)
                                                    except Exception:
                                                        # best-effort: only remove if it's safe
                                                        try:
                                                            os.remove(dst)
                                                        except Exception:
                                                            pass
                                                if dst:
                                                    shutil.move(cand_abs, dst)
                                                    log.info(f"Recovered and moved candidate {cand_abs} -> {dst}")
                                                    moved = True
                                                    recovered_moved = True
                                                    # Verify destination exists immediately; if missing, search for likely locations
                                                    try:
                                                        if not os.path.exists(dst):
                                                            log.warning(f"Post-move check: destination missing after move: {dst}")
                                                            bname = os.path.basename(dst)
                                                            # First search temp and completed dirs
                                                            found = []
                                                            try:
                                                                roots = []
                                                                if completed_dir and os.path.exists(completed_dir):
                                                                    roots.append(completed_dir)
                                                                if temp_dir and os.path.exists(temp_dir):
                                                                    roots.append(temp_dir)
                                                                for r in roots:
                                                                    for root, _, files_search in os.walk(r):
                                                                        for fn_search in files_search:
                                                                            if fn_search == bname or bname in fn_search:
                                                                                found.append(os.path.join(root, fn_search))
                                                            except Exception:
                                                                log.debug("Error while searching for post-move file in local dirs")
                                                            # If nothing found, do a bounded drive-wide search (J: drive etc.)
                                                            if not found:
                                                                try:
                                                                    drive = os.path.splitdrive(abs_completed)[0] or os.path.splitdrive(completed_dir)[0]
                                                                    if drive:
                                                                        drive_root = drive + os.sep
                                                                        # bounded search: limit files visited and time
                                                                        max_files = 20000
                                                                        files_seen = 0
                                                                        import time as _time
                                                                        start = _time.time()
                                                                        for root, _, files_search in os.walk(drive_root):
                                                                            for fn_search in files_search:
                                                                                if fn_search == bname or bname in fn_search:
                                                                                    found.append(os.path.join(root, fn_search))
                                                                                files_seen += 1
                                                                                if files_seen >= max_files or (_time.time() - start) > 2.0:
                                                                                    break
                                                                            if files_seen >= max_files or (_time.time() - start) > 2.0:
                                                                                break
                                                                except Exception:
                                                                    log.debug("Error during bounded drive-wide search")
                                                            log.debug(f"Post-move search results for {bname}: {found}")
                                                    except Exception:
                                                        log.debug("Error during post-move existence check")
                                                    break
                                            except FileNotFoundError:
                                                log.debug(f"Candidate disappeared during recovery move: {cand_abs}")
                                                moved = True
                                                break
                                            except OSError as e:
                                                winerr = getattr(e, 'winerror', None)
                                                errnum = getattr(e, 'errno', None)
                                                if winerr == 32 or errnum in (13,):
                                                    __import__('time').sleep(0.2 * (attempt + 1))
                                                    continue
                                                else:
                                                    log.error(f"Failed to move candidate {cand_abs} to {dst}: {e}")
                                                    break
                                        if not moved:
                                            log.debug(f"Recovery move failed after retries for {cand_abs} -> {dst}")
                                    except Exception as e:
                                        log.exception(f"Error during candidate recovery move: {e}")
                                else:
                                    log.debug(f"No candidate files found for recovery tokens: {search_tokens}")
                            except Exception:
                                log.exception("Error during missing-source recovery logic")
                            # If recovery moved a candidate into completed, skip further processing
                            if recovered_moved:
                                continue

                            # If source is inside temp dir, we should move it to completed
                            try:
                                if abs_temp and os.path.commonpath([abs_src, abs_temp]) == abs_temp:
                                    in_temp = True
                                else:
                                    in_temp = False
                            except Exception:
                                in_temp = False

                            # If source is already in completed dir and NOT in temp, skip moving
                            try:
                                if abs_completed and os.path.commonpath([abs_src, abs_completed]) == abs_completed and not in_temp:
                                    log.debug(f"Source already in completed directory, skipping move: {src}")
                                    continue
                            except Exception:
                                pass

                            # Wait for file to be stable (size/mtime unchanged) before moving
                            try:
                                if not self._wait_for_file_stable(src, timeout=10):
                                    log.warning(f"File {src} did not stabilize before move; proceeding anyway")
                            except Exception:
                                log.exception(f"Error while waiting for file to stabilize: {src}")

                            # Build destination path and normalize it (avoid mixed separators)
                            dst = os.path.normpath(os.path.join(abs_completed, os.path.basename(abs_src))) if abs_completed else None
                            # If destination exists, replace it
                            try:
                                if dst and os.path.exists(dst):
                                    try:
                                        # Don't remove dst if it's the same file as the
                                        # source we're about to move (avoid deleting it).
                                        if os.path.abspath(dst) != os.path.abspath(abs_src):
                                            os.remove(dst)
                                    except Exception:
                                        try:
                                            os.remove(dst)
                                        except Exception:
                                            pass
                                if dst:
                                    # Retry on common Windows sharing/lock errors (winerror 32) and permission errors
                                    moved = False
                                    for attempt in range(5):
                                        try:
                                            shutil.move(abs_src, dst)
                                            log.info(f"Moved {abs_src} -> {dst}")
                                            # Verify destination exists immediately; if missing, search for likely locations
                                            try:
                                                if not os.path.exists(dst):
                                                    log.warning(f"Post-move check: destination missing after move: {dst}")
                                                    bname = os.path.basename(dst)
                                                    found = []
                                                    try:
                                                        roots = []
                                                        if completed_dir and os.path.exists(completed_dir):
                                                            roots.append(completed_dir)
                                                        if temp_dir and os.path.exists(temp_dir):
                                                            roots.append(temp_dir)
                                                        for r in roots:
                                                            for root, _, files_search in os.walk(r):
                                                                for fn_search in files_search:
                                                                    if fn_search == bname or bname in fn_search:
                                                                        found.append(os.path.join(root, fn_search))
                                                    except Exception:
                                                        log.debug("Error during post-move search")
                                                    log.debug(f"Post-move search results for {bname}: {found}")
                                            except Exception:
                                                log.debug("Error during post-move existence check")
                                            moved = True
                                            break
                                        except FileNotFoundError:
                                            # File vanished between discovery and move; likely cleaned up by yt-dlp.
                                            log.debug(f"Source disappeared before move, skipping: {abs_src}")
                                            moved = True
                                            break
                                        except OSError as e:
                                            winerr = getattr(e, 'winerror', None)
                                            errnum = getattr(e, 'errno', None)
                                            # Retry for sharing violation / permission temporarily busy
                                            if winerr == 32 or errnum in (13,):
                                                __import__('time').sleep(0.2 * (attempt + 1))
                                                continue
                                            else:
                                                log.error(f"Failed to move {abs_src} to {dst}: {e}")
                                                break
                                    if not moved:
                                        log.debug(f"Move failed after retries, skipping: {abs_src} -> {dst}")
                            except Exception as e:
                                log.error(f"Failed to prepare move for {abs_src} to {dst}: {e}")
                            except Exception as e:
                                log.error(f"Failed to prepare move for {abs_src} to {dst}: {e}")
                    except Exception as e:
                        log.debug(f"Skipping file move for {f}: {e}")
                # After attempting moves, try to remove any leftover .part files reported
                try:
                    for f in files:
                        try:
                            p = os.path.normpath(str(f).strip().strip('\"\''))
                        except Exception:
                            p = None
                        if not p:
                            continue
                        try:
                            if p.lower().endswith('.part') and os.path.exists(p):
                                # Only remove part files from temp dir to avoid touching user data
                                if temp_dir and os.path.commonpath([os.path.abspath(p), os.path.abspath(temp_dir)]) == os.path.abspath(temp_dir):
                                    try:
                                        os.remove(p)
                                        log.info(f"Removed leftover part file: {p}")
                                    except Exception:
                                        log.debug(f"Could not remove leftover part file (in use?): {p}")
                        except Exception:
                            pass
                except Exception:
                    pass
        except Exception:
            log.exception("Error while moving completed files")

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
                    for root, _, files in os.walk(completed_dir):
                        for fn in files:
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
        except Exception:
            user_message = None

        # Fallback to original message if no mapping found
        emit_msg = user_message if user_message else message
        self.download_error.emit(url, emit_msg)

        # Start next queued download if any
        self._maybe_start_next()
