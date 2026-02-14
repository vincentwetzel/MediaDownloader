import logging
import os
import subprocess
import threading
import sys
import shutil
import json
import requests
import tempfile
import re
import platform
import time
from PyQt6.QtCore import QThread, pyqtSignal
from core.binary_manager import get_binary_path, get_ffmpeg_location
from core.yt_dlp_args_builder import build_yt_dlp_args

# --- HIDE CONSOLE WINDOW for SUBPROCESS ---
creation_flags = 0
if sys.platform == "win32" and getattr(sys, "frozen", False):
    creation_flags = subprocess.CREATE_NO_WINDOW

log = logging.getLogger(__name__)

# Global variable to store the working yt-dlp path
_YT_DLP_PATH = None
_GALLERY_DL_PATH = None
_YT_DLP_VERSION_CACHE = None


def check_yt_dlp_available():
    """Check if yt-dlp is available by consulting the binary manager and verifying the executable."""
    global _YT_DLP_PATH

    yt_dlp_path = get_binary_path("yt-dlp")

    if not yt_dlp_path:
        _YT_DLP_PATH = None
        return False, "yt-dlp executable not found in bundled binaries."

    try:
        log.debug(f"Attempting to verify yt-dlp at: {yt_dlp_path}")
        result = subprocess.run(
            [yt_dlp_path, "--version"],
            capture_output=True, text=True, timeout=5, shell=False,
            errors='replace', stdin=subprocess.DEVNULL, creationflags=creation_flags
        )

        if result.returncode == 0:
            version = result.stdout.strip()
            _YT_DLP_PATH = yt_dlp_path
            log.info(f"Working yt-dlp found at: {_YT_DLP_PATH}, version: {version}")
            return True, f"yt-dlp found at: {_YT_DLP_PATH}, version: {version}"
        else:
            stderr = (result.stderr or "").strip()
            stdout = (result.stdout or "").strip()
            log.warning(f"yt-dlp at {yt_dlp_path} --version failed. RC: {result.returncode}, Stderr: {stderr}")
            _YT_DLP_PATH = None
            extra = []
            if stdout:
                extra.append(f"stdout: {stdout}")
            if stderr:
                extra.append(f"stderr: {stderr}")
            extra_msg = f" ({'; '.join(extra)})" if extra else ""
            return False, f"yt-dlp at {yt_dlp_path} failed verification (rc={result.returncode}).{extra_msg}"

    except Exception as e:
        log.error(f"Error verifying yt-dlp at {yt_dlp_path}: {str(e)}")
        _YT_DLP_PATH = None
        return False, f"Error verifying yt-dlp at {yt_dlp_path}."


def check_gallery_dl_available():
    """Check if gallery-dl is available by consulting the binary manager and verifying the executable."""
    global _GALLERY_DL_PATH

    gallery_dl_path = get_binary_path("gallery-dl")

    if not gallery_dl_path:
        _GALLERY_DL_PATH = None
        return False, "gallery-dl executable not found in bundled binaries."

    try:
        log.debug(f"Attempting to verify gallery-dl at: {gallery_dl_path}")
        result = subprocess.run(
            [gallery_dl_path, "--version"],
            capture_output=True, text=True, timeout=5, shell=False,
            errors='replace', stdin=subprocess.DEVNULL, creationflags=creation_flags
        )

        if result.returncode == 0:
            version = result.stdout.strip()
            _GALLERY_DL_PATH = gallery_dl_path
            log.info(f"Working gallery-dl found at: {_GALLERY_DL_PATH}, version: {version}")
            return True, f"gallery-dl found at: {_GALLERY_DL_PATH}, version: {version}"
        else:
            stderr = (result.stderr or "").strip()
            stdout = (result.stdout or "").strip()
            log.warning(f"gallery-dl at {gallery_dl_path} --version failed. RC: {result.returncode}, Stderr: {stderr}")
            _GALLERY_DL_PATH = None
            extra = []
            if stdout:
                extra.append(f"stdout: {stdout}")
            if stderr:
                extra.append(f"stderr: {stderr}")
            extra_msg = f" ({'; '.join(extra)})" if extra else ""
            return False, f"gallery-dl at {gallery_dl_path} failed verification (rc={result.returncode}).{extra_msg}"

    except Exception as e:
        log.error(f"Error verifying gallery-dl at {gallery_dl_path}: {str(e)}")
        _GALLERY_DL_PATH = None
        return False, f"Error verifying gallery-dl at {gallery_dl_path}."


def get_yt_dlp_version(force_check=False):
    """
    Return the version string of the current yt-dlp executable.
    Uses a cache unless force_check is True.
    """
    global _YT_DLP_PATH, _YT_DLP_VERSION_CACHE
    
    if not force_check and _YT_DLP_VERSION_CACHE:
        return _YT_DLP_VERSION_CACHE

    try:
        if not _YT_DLP_PATH or force_check:
            log.debug("get_yt_dlp_version: _YT_DLP_PATH not set or check is forced, checking availability...")
            check_yt_dlp_available()
        
        if _YT_DLP_PATH:
            try:
                log.debug(f"Fetching version for: {_YT_DLP_PATH}")
                result = subprocess.run(
                    [_YT_DLP_PATH, "--version"],
                    capture_output=True,
                    text=True,
                    timeout=5,
                    shell=False,
                    errors='replace',
                    stdin=subprocess.DEVNULL,
                    creationflags=creation_flags
                )
                if result.returncode == 0:
                    ver = result.stdout.strip()
                    log.debug(f"Version fetched: {ver}")
                    _YT_DLP_VERSION_CACHE = ver
                    return ver
                else:
                    log.warning(f"Failed to get version. RC: {result.returncode}, Stderr: {result.stderr}")
                    _YT_DLP_VERSION_CACHE = None
                    return None
            except subprocess.TimeoutExpired:
                log.warning("get_yt_dlp_version: subprocess timed out")
                _YT_DLP_VERSION_CACHE = None
                return None
            except Exception as e:
                log.error(f"Exception getting version: {e}")
                _YT_DLP_VERSION_CACHE = None
                return None
        else:
            log.warning("get_yt_dlp_version: No yt-dlp path found.")
            _YT_DLP_VERSION_CACHE = None
            return None
    except Exception as outer_e:
        log.error(f"Unexpected error in get_yt_dlp_version: {outer_e}")
        _YT_DLP_VERSION_CACHE = None
        return None


def fetch_metadata(url: str, timeout: int = 15, noplaylist: bool = False):
    """Fetch metadata for a URL using yt-dlp --dump-single-json.

    Returns parsed JSON dict on success, or None on failure.
    This function is safe to call from background threads.
    """
    try:
        yt_dlp_cmd = get_binary_path("yt-dlp")
        if not yt_dlp_cmd:
            log.error("fetch_metadata: yt-dlp binary not found.")
            return None
        
        # Added --no-input to prevent interactive prompts
        # Added --no-cache-dir to prevent locking issues
        # Added --no-write-playlist-metafiles to avoid clutter
        meta_cmd = [yt_dlp_cmd, "--dump-single-json", "--no-cache-dir", "--no-write-playlist-metafiles", "--no-input"]
        if noplaylist:
            meta_cmd.append("--no-playlist")
        else:
            # Use flat-playlist to avoid resolving all videos in a playlist, which causes hangs
            meta_cmd.append("--flat-playlist")

        meta_cmd.append(url)
        
        log.debug(f"fetch_metadata running: {meta_cmd}")
        proc = subprocess.run(meta_cmd, capture_output=True, text=True, timeout=timeout, shell=False, stdin=subprocess.DEVNULL, creationflags=creation_flags)
        if proc.returncode == 0 and proc.stdout:
            try:
                info = json.loads(proc.stdout)
                return info
            except Exception:
                return None
        return None
    except Exception as e:
        log.warning(f"fetch_metadata failed: {e}")
        return None


def is_url_valid(url: str, timeout: int = 15, noplaylist: bool = False):
    """Check if a URL is valid using yt-dlp --simulate.
    Returns True if valid, False otherwise.
    """
    try:
        yt_dlp_cmd = get_binary_path("yt-dlp")
        if not yt_dlp_cmd:
            log.error("is_url_valid: yt-dlp binary not found.")
            return False
        # --simulate: do not download the video and do not write anything to disk
        cmd = [yt_dlp_cmd, "--simulate", "--no-cache-dir", "--no-input"]
        if noplaylist:
            cmd.append("--no-playlist")
        cmd.append(url)
        
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, shell=False, stdin=subprocess.DEVNULL, creationflags=creation_flags)
        return proc.returncode == 0
    except Exception:
        return False


def is_gallery_url_valid(url: str, timeout: int = 15):
    """Check if a URL is valid using gallery-dl --simulate.
    Returns True if valid, False otherwise.
    """
    try:
        gallery_dl_cmd = get_binary_path("gallery-dl")
        if not gallery_dl_cmd:
            log.error("is_gallery_url_valid: gallery-dl binary not found.")
            return False
        # --simulate: do not download
        # --quiet: suppress output
        # NOTE: gallery-dl --simulate might fail if it can't extract info, but it's the best check we have.
        # Some gallery-dl extractors might require cookies or other config.
        cmd = [gallery_dl_cmd, "--simulate", url]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, shell=False, stdin=subprocess.DEVNULL, creationflags=creation_flags)
        
        # If return code is 0, it's valid.
        if proc.returncode == 0:
            return True
            
        # If it failed, check stderr. Sometimes it fails due to 403 but the URL is technically valid for the extractor.
        # However, for our purpose, if we can't simulate, we probably can't download.
        log.warning(f"gallery-dl validation failed for {url}. RC: {proc.returncode}, Stderr: {proc.stderr}")
        return False
    except Exception:
        return False


class DownloadWorker(QThread):
    progress = pyqtSignal(dict)
    title_updated = pyqtSignal(str)
    finished = pyqtSignal(str, bool, list)
    error = pyqtSignal(str, str)

    def __init__(self, url, opts, config_manager, parent=None):
        super().__init__(parent)
        self.url = url
        self.opts = opts
        self.config_manager = config_manager
        self._is_cancelled = False

    def _parse_error_output(self, error_output):
        """Parse yt-dlp error output for known patterns and generate a user-friendly message."""
        
        full_error_text = "\n".join(error_output)

        # Pattern for "Premieres in..."
        premiere_match = re.search(r"ERROR:.*? premieres in (.+)", full_error_text, re.IGNORECASE | re.DOTALL)
        if premiere_match:
            return f"This video is a premiere and will be available in {premiere_match.group(1).strip()}."
        
        # Add more patterns here in the future...

        return None # No specific error found

    def run(self):
        error_output = []
        stdout_lines = []
        try:
            # Determine if we should use gallery-dl or yt-dlp
            use_gallery_dl = self.opts.get("use_gallery_dl", False)

            # Build arguments internally
            try:
                cmd_args = build_yt_dlp_args(self.opts, self.config_manager)
            except ValueError as e:
                self.error.emit(self.url, str(e))
                return

            if use_gallery_dl:
                global _GALLERY_DL_PATH
                is_available, status_msg = check_gallery_dl_available()
                log.info(f"gallery-dl check: {status_msg}")
                if not is_available:
                    error_msg = f"gallery-dl not available: {status_msg}"
                    log.error(error_msg)
                    self.error.emit(self.url, error_msg)
                    return
                
                cmd_executable = _GALLERY_DL_PATH
                cmd = [cmd_executable] + cmd_args
                cmd.append(self.url)

            else:
                # First, verify yt-dlp is available
                global _YT_DLP_PATH
                is_available, status_msg = check_yt_dlp_available()
                log.info(f"yt-dlp check: {status_msg}")
                if not is_available:
                    error_msg = f"yt-dlp not available: {status_msg}"
                    log.error(error_msg)
                    self.error.emit(self.url, error_msg)
                    return
                
                cmd_executable = _YT_DLP_PATH

                # Attempt to extract metadata (title) first
                if not getattr(self, "_meta_info", None):
                    log.info(f"Fetching metadata for {self.url}...")
                    try:
                        # Respect playlist mode to avoid hanging on large playlists
                        playlist_mode = self.opts.get("playlist_mode", "Ask")
                        noplaylist = False
                        if "ignore" in playlist_mode.lower() or "single" in playlist_mode.lower():
                            noplaylist = True
                        
                        info = fetch_metadata(self.url, timeout=20, noplaylist=noplaylist)
                        if info:
                            self._meta_info = info
                            title = info.get("title") or info.get("id")
                            if title:
                                self.title_updated.emit(str(title))
                            log.info("Metadata fetch successful.")
                        else:
                            log.info("Metadata fetch returned None.")
                    except Exception as e:
                        log.warning(f"Metadata fetch failed: {e}")
                else:
                    log.info("Metadata already available, skipping fetch.")

                cmd = [cmd_executable] + cmd_args

                # Add subtitle options
                embed_subs = self.config_manager.get("General", "subtitles_embed", fallback="False") == "True"
                write_subs = self.config_manager.get("General", "subtitles_write", fallback="False") == "True"
                write_auto_subs = self.config_manager.get("General", "subtitles_write_auto", fallback="False") == "True"
                
                sub_langs = self.config_manager.get("General", "subtitles_langs", fallback="en")
                sub_format = self.config_manager.get("General", "subtitles_format", fallback="srt")
                embed_chapters = self.config_manager.get("General", "embed_chapters", fallback="True") == "True"

                if embed_subs:
                    cmd.append("--embed-subs")
                if write_subs:
                    cmd.append("--write-subs")
                if write_auto_subs:
                    cmd.append("--write-auto-subs")
                if embed_chapters:
                    cmd.append("--embed-chapters")

                if embed_subs or write_subs or write_auto_subs:
                    if sub_langs:
                        cmd.extend(["--sub-langs", sub_langs])
                    if sub_format and sub_format.lower() != 'none':
                        cmd.extend(["--convert-subs", sub_format])

                # Check for external downloader setting
                external_downloader = self.config_manager.get("General", "external_downloader", fallback="none")
                if external_downloader == "aria2":
                    aria2c_path = get_binary_path("aria2c")
                    if aria2c_path:
                        cmd.extend(["--external-downloader", aria2c_path])
                    else:
                        log.warning("aria2c not found in bundled binaries. Skipping external downloader.")

                ffmpeg_location = get_ffmpeg_location()
                if ffmpeg_location:
                    cmd.extend(["--ffmpeg-location", ffmpeg_location])
                else:
                    log.warning("No bundled ffmpeg location resolved.")

                cmd.append(self.url)

            log.info(f"Running command: {' '.join(cmd)}")

            temp_snapshot = {}
            temp_dir = getattr(self, "temp_dir", None)
            if temp_dir and os.path.isdir(temp_dir):
                try:
                    for fn in os.listdir(temp_dir):
                        if fn.startswith("preferred_thumb_") and fn.lower().endswith(('.jpg', '.jpeg', '.png')):
                            os.remove(os.path.join(temp_dir, fn))
                except Exception:
                    pass
                try:
                    for root, _, files in os.walk(temp_dir):
                        for f in files:
                            p = os.path.join(root, f)
                            try:
                                temp_snapshot[p] = os.path.getmtime(p)
                            except Exception:
                                temp_snapshot[p] = 0
                except Exception:
                    pass

            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                bufsize=1,
                errors='replace',
                shell=False,
                stdin=subprocess.DEVNULL,
                creationflags=creation_flags
            )

            def read_stream(stream, is_stderr=False):
                try:
                    for line in stream:
                        if self._is_cancelled:
                            process.terminate()
                            return
                        line = line.strip()
                        if line:
                            if is_stderr:
                                error_output.append(line)
                                log.warning(f"{self.url} stderr: {line}")
                            else:
                                stdout_lines.append(line)
                                if any(keyword in line.lower() for keyword in ["error", "failed", "unable", "cannot", "invalid", "not found", "unavailable"]):
                                    error_output.append(line)
                                if not any(keyword in line.lower() for keyword in ["error", "failed"]):
                                    self.progress.emit({"url": self.url, "text": line})
                                log.debug(f"{self.url} stdout: {line}")
                except Exception as e:
                    log.warning(f"Error reading stream: {e}")
            
            stdout_thread = threading.Thread(target=lambda: read_stream(process.stdout, False), daemon=True)
            stderr_thread = threading.Thread(target=lambda: read_stream(process.stderr, True), daemon=True)
            stdout_thread.start()
            stderr_thread.start()
            
            return_code = process.wait()
            
            stdout_thread.join(timeout=2.0)
            stderr_thread.join(timeout=2.0)

            if return_code == 0:
                time.sleep(1)
            
            if not stdout_lines and not error_output:
                try:
                    stdout_data, stderr_data = process.communicate(timeout=1)
                    if stdout_data:
                        stdout_lines.extend(l.strip() for l in stdout_data.strip().split('\n') if l.strip())
                    if stderr_data:
                        error_output.extend(l.strip() for l in stderr_data.strip().split('\n') if l.strip())
                except Exception:
                    pass

            if self._is_cancelled:
                log.info(f"Download cancelled by user: {self.url}")
                self.finished.emit(self.url, False, [])
                return

            if return_code == 0:
                time.sleep(1)
                created_files = []
                final_file_from_stdout = None
                
                if use_gallery_dl:
                    # gallery-dl output parsing
                    for line in stdout_lines:
                        fpath = line.strip()
                        if not fpath or fpath.startswith('[') or fpath.startswith('#'):
                            continue
                        
                        # Check if it exists as is
                        if os.path.exists(fpath):
                            created_files.append(os.path.normpath(fpath))
                            continue
                        
                        # Check relative to temp_dir/output_dir
                        t_dir = getattr(self, "temp_dir", "")
                        c_dir = self.config_manager.get("Paths", "completed_downloads_directory", fallback="")
                        target_dir = t_dir or c_dir
                        
                        if target_dir:
                            cand = os.path.join(target_dir, fpath)
                            if os.path.exists(cand):
                                created_files.append(os.path.normpath(cand))
                else:
                    try:
                        for line in reversed(stdout_lines):
                            m = re.search(r"Merging formats into\s*(.+)$", line)
                            if m:
                                final_file_from_stdout = m.group(1).strip()
                                break
                            if not final_file_from_stdout:
                                m2 = re.search(r"Destination:\s*(.+)$", line)
                                if m2:
                                    final_file_from_stdout = m2.group(1).strip()
                    except Exception:
                        log.exception("Error parsing yt-dlp stdout for filenames.")

                if final_file_from_stdout:
                    if (final_file_from_stdout.startswith('"') and final_file_from_stdout.endswith('"')) or \
                       (final_file_from_stdout.startswith("'") and final_file_from_stdout.endswith("'")):
                        final_file_from_stdout = final_file_from_stdout[1:-1].strip()
                    created_files.append(os.path.normpath(final_file_from_stdout))
                    log.info(f"Discovered final file from stdout: {created_files}")
                else:
                    if not use_gallery_dl:
                        log.warning(f"Could not determine final file from output for {self.url}. Using directory snapshot fallback.")
                    snapshot_files = []
                    try:
                        temp_dir = getattr(self, "temp_dir", None)
                        if temp_dir and os.path.isdir(temp_dir):
                            for root, _, files in os.walk(temp_dir):
                                for f in files:
                                    p = os.path.join(root, f)
                                    try:
                                        mtime = os.path.getmtime(p)
                                    except Exception:
                                        continue
                                    if p not in temp_snapshot or mtime > temp_snapshot.get(p, 0):
                                        snapshot_files.append(p)
                    except Exception:
                        pass
                    
                    seen = set()
                    for p in snapshot_files:
                        try:
                            sp = str(p).strip().strip('"').strip("'").strip()
                            if not sp or sp.lower().endswith(('.part', '.ytdl')):
                                continue
                            sp = os.path.normpath(sp)
                            if sp in seen:
                                continue
                            seen.add(sp)
                            created_files.append(sp)
                        except Exception:
                            continue
                    if not use_gallery_dl:
                        log.info(f"Discovered final files from snapshot: {created_files}")
                    else:
                        # For gallery-dl, if we found files via stdout parsing, we append them.
                        # If we didn't find any via stdout, we might want to use snapshot files too?
                        # But gallery-dl usually downloads many files.
                        # If created_files is empty, we can try snapshot.
                        if not created_files:
                            for f in snapshot_files:
                                if f not in created_files:
                                    created_files.append(f)

                if not use_gallery_dl:
                    try:
                        self._handle_thumbnail_embedding(created_files)
                    except Exception:
                        log.exception("Thumbnail handling failed")

                self.finished.emit(self.url, True, created_files)
            else:
                error_msg = self._parse_error_output(error_output) or f"Process error (code {return_code})"
                if "error" in error_msg.lower():
                    if error_output:
                        relevant_errors = error_output[-5:]
                        error_msg += f"\n\n{''.join(relevant_errors)}"
                    elif stdout_lines:
                        error_lines = [line for line in stdout_lines if "error" in line.lower()]
                        if error_lines:
                            error_msg += f"\n\n{''.join(error_lines[-3:])}"
                        else:
                            error_msg += f"\n\nLast output: {''.join(stdout_lines[-3:])}"
                
                log.error(f"Download failed for {self.url} with code {return_code}")
                log.error(f"Command: {' '.join(cmd)}")
                if error_output:
                    log.error(f"Error output: {''.join(error_output)}")

                self.error.emit(self.url, error_msg)
        except FileNotFoundError:
            error_msg = "Executable not found. Please check your installation."
            log.error(error_msg)
            self.error.emit(self.url, error_msg)
        except Exception as e:
            error_msg = f"Download error: {str(e)}"
            if error_output:
                error_msg += f" | {' '.join(error_output[-2:])}"
            log.exception(error_msg)
            self.error.emit(self.url, error_msg)

    def cancel(self):
        self._is_cancelled = True

    def _handle_thumbnail_embedding(self, created_files):
        ffmpeg_path = get_binary_path("ffmpeg")
        ffprobe_path = get_binary_path("ffprobe")

        if not ffmpeg_path or not ffprobe_path:
            log.warning("ffmpeg or ffprobe not found. Skipping thumbnail embedding.")
            return

        try:
            info = getattr(self, "_meta_info", None)
            if not info:
                return

            vid_id = info.get("id")
            thumbs = info.get("thumbnails", [])
            thumb_url = max(thumbs, key=lambda t: (t.get("width") or 0) * (t.get("height") or 0), default={}).get("url") or info.get("thumbnail")

            if not thumb_url:
                return

            candidates = [(thumb_url, "original")]
            if "hqdefault" in thumb_url:
                candidates.append((thumb_url.replace("hqdefault", "maxresdefault"), "maxresdefault"))

            headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"}
            download_dir = getattr(self, "temp_dir", os.getcwd())
            os.makedirs(download_dir, exist_ok=True)

            downloaded_thumb = None
            created_temp_files = []
            for cand_url, _ in candidates:
                try:
                    r = requests.get(cand_url, stream=True, headers=headers, timeout=12)
                    if r.status_code == 200:
                        with tempfile.NamedTemporaryFile(delete=False, suffix='.jpg', dir=download_dir) as tf:
                            for chunk in r.iter_content(1024 * 8):
                                tf.write(chunk)
                            downloaded_thumb = tf.name
                        created_temp_files.append(downloaded_thumb)
                        break
                except Exception:
                    pass

            if not downloaded_thumb:
                return

            target = next((os.path.normpath(p.strip().strip('"\'')) for p in created_files if os.path.exists(os.path.normpath(p.strip().strip('"\'')))), None)
            if not target:
                # Cleanup and exit if no valid media file was found
                for p in created_temp_files:
                    if os.path.exists(p): os.remove(p)
                return

            thumb_jpg = downloaded_thumb
            if not thumb_jpg.lower().endswith((".jpg", ".jpeg")):
                conv = os.path.join(download_dir, f"embed_thumb_{vid_id}.jpg")
                try:
                    subprocess.run([ffmpeg_path, "-y", "-i", downloaded_thumb, conv], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=creation_flags)
                    thumb_jpg = conv
                    created_temp_files.append(conv)
                except Exception:
                    pass

            out_tmp = f"{os.path.splitext(target)[0]}.embedtmp{os.path.splitext(target)[1]}"

            cmd = [
                ffmpeg_path, "-y", "-i", target, "-i", thumb_jpg,
                "-map", "0", "-map", "1", "-c", "copy",
                "-metadata:s:v:1", "title=Album cover",
                "-metadata:s:v:1", "comment=Cover (front)",
                "-disposition:v:1", "attached_pic", out_tmp,
            ]

            try:
                subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=creation_flags)
                os.replace(out_tmp, target)
            except subprocess.CalledProcessError as e:
                stderr = e.stderr.decode(errors='replace') if isinstance(e.stderr, bytes) else str(e.stderr)
                log.error("ffmpeg embedding failed for %s: %s", target, stderr)
            finally:
                # Cleanup all temporary files
                for p in created_temp_files:
                    if os.path.exists(p): os.remove(p)
                if os.path.exists(out_tmp): os.remove(out_tmp)

        except Exception as e:
            log.exception(f"Error in thumbnail embedding helper: {e}")
