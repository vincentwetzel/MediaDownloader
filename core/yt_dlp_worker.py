import logging
import os
import subprocess
import threading
import sys
import shutil
import json
import requests
import tempfile
from PyQt6.QtCore import QThread, pyqtSignal

log = logging.getLogger(__name__)

# Global variable to store the working yt-dlp path
_YT_DLP_PATH = None


def check_yt_dlp_available():
    """Check if yt-dlp is available and return version info. Tries all found versions."""
    global _YT_DLP_PATH
    
    candidate_paths = []

    # 1. Check if yt-dlp is in PATH using shutil.which
    yt_dlp_in_path = shutil.which("yt-dlp")
    if yt_dlp_in_path:
        candidate_paths.append(yt_dlp_in_path.strip())

    # 2. Add common Python Scripts directories for yt-dlp.exe on Windows
    #    This covers both system-wide and user-specific installations
    #    Iterate through common Python versions to be more robust
    #    We check a range of versions to be as compatible as possible without hardcoding just one.
    #    This list can be extended as new Python versions are released.
    python_versions = [f"Python3{i}" for i in range(15, 9, -1)] # Checks Python 3.15 down to 3.10

    for version in python_versions:
        # System-wide (e.g., C:\Program Files\PythonXX\Scripts)
        prog_files_python_scripts = os.path.join(
            os.environ.get("PROGRAMFILES", r"C:\Program Files"),
            version, "Scripts", "yt-dlp.exe"
        )
        if os.path.exists(prog_files_python_scripts):
            candidate_paths.append(prog_files_python_scripts)

        # User-specific AppData (e.g., C:\Users\User\AppData\Local\Programs\Python\PythonXX\Scripts)
        local_app_data_python_scripts = os.path.join(
            os.environ.get("LOCALAPPDATA", ""),
            "Programs", "Python", version, "Scripts", "yt-dlp.exe"
        )
        if os.path.exists(local_app_data_python_scripts):
            candidate_paths.append(local_app_data_python_scripts)
            
        # User-specific AppData Roaming (e.g., C:\Users\User\AppData\Roaming\Python\PythonXX\Scripts)
        roaming_app_data_python_scripts = os.path.join(
            os.environ.get("APPDATA", ""),
            "Python", version, "Scripts", "yt-dlp.exe"
        )
        if os.path.exists(roaming_app_data_python_scripts):
            candidate_paths.append(roaming_app_data_python_scripts)

    # 3. Add explicit common direct installation paths for yt-dlp.exe
    common_direct_paths = [
        r"C:\yt-dlp\yt-dlp.exe",
        r"C:\Program Files\yt-dlp\yt-dlp.exe",
        r"C:\Program Files (x86)\yt-dlp\yt-dlp.exe",
        os.path.join(os.environ.get("LOCALAPPDATA", ""), "yt-dlp", "yt-dlp.exe"),
    ]
    for path in common_direct_paths:
        if os.path.exists(path):
            candidate_paths.append(path)

    # 4. Check for bundled yt-dlp.exe (for PyInstaller builds)
    if getattr(sys, 'frozen', False):
        # If running as a PyInstaller bundle, check the _MEIPASS directory or the executable directory
        bundled_path = os.path.join(sys._MEIPASS, "yt-dlp.exe")
        if os.path.exists(bundled_path):
            candidate_paths.insert(0, bundled_path) # Prioritize bundled version
        
        exe_dir_path = os.path.join(os.path.dirname(sys.executable), "yt-dlp.exe")
        if os.path.exists(exe_dir_path):
            candidate_paths.append(exe_dir_path)

    # Remove duplicates and ensure .exe on Windows if missing
    final_candidate_paths = []
    seen_paths = set()
    for path in candidate_paths:
        normalized_path = path
        if os.name == 'nt' and not normalized_path.lower().endswith('.exe') and os.path.exists(normalized_path + '.exe'):
            normalized_path += '.exe'
        if normalized_path not in seen_paths:
            final_candidate_paths.append(normalized_path)
            seen_paths.add(normalized_path)

    log.info(f"Checking for yt-dlp in candidates: {final_candidate_paths}")

    for yt_dlp_path in final_candidate_paths:
        if not os.path.exists(yt_dlp_path):
            continue
        try:
            log.debug(f"Attempting to verify yt-dlp at: {yt_dlp_path}")
            result = subprocess.run(
                [yt_dlp_path, "--version"],
                capture_output=True,
                text=True,
                timeout=5,
                shell=False,
                errors='replace',
                stdin=subprocess.DEVNULL
            )

            if result.returncode == 0:
                version = result.stdout.strip()
                _YT_DLP_PATH = yt_dlp_path
                log.info(f"Working yt-dlp found at: {_YT_DLP_PATH}, version: {version}")
                return True, f"yt-dlp found at: {_YT_DLP_PATH}, version: {version}"
            else:
                log.warning(f"yt-dlp at {yt_dlp_path} --version failed. Return code: {result.returncode}, Stderr: {result.stderr.strip()}")
        except FileNotFoundError:
            log.debug(f"yt-dlp executable not found at {yt_dlp_path} during verification.")
        except subprocess.TimeoutExpired:
            log.warning(f"yt-dlp at {yt_dlp_path} --version timed out.")
        except Exception as e:
            log.error(f"Error verifying yt-dlp at {yt_dlp_path}: {str(e)}")

    _YT_DLP_PATH = None
    return False, "yt-dlp executable not found or not working. Please ensure it's installed and in your system's PATH, or at a common install location."


def get_yt_dlp_version():
    """Return the version string of the current yt-dlp executable, or None if not found."""
    global _YT_DLP_PATH
    if not _YT_DLP_PATH:
        # Try to find it if not already found
        log.debug("get_yt_dlp_version: _YT_DLP_PATH not set, checking availability...")
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
                stdin=subprocess.DEVNULL
            )
            if result.returncode == 0:
                ver = result.stdout.strip()
                log.debug(f"Version fetched: {ver}")
                return ver
            else:
                log.warning(f"Failed to get version. RC: {result.returncode}, Stderr: {result.stderr}")
        except Exception as e:
            log.error(f"Exception getting version: {e}")
            pass
    else:
        log.warning("get_yt_dlp_version: No yt-dlp path found.")
    return None


def fetch_metadata(url: str, timeout: int = 15):
    """Fetch metadata for a URL using yt-dlp --dump-single-json.

    Returns parsed JSON dict on success, or None on failure.
    This function is safe to call from background threads.
    """
    global _YT_DLP_PATH
    try:
        yt_dlp_cmd = _YT_DLP_PATH if _YT_DLP_PATH else shutil.which("yt-dlp") or "yt-dlp"
        meta_cmd = [yt_dlp_cmd, "--dump-single-json", url]
        proc = subprocess.run(meta_cmd, capture_output=True, text=True, timeout=timeout, shell=False, stdin=subprocess.DEVNULL)
        if proc.returncode == 0 and proc.stdout:
            try:
                info = json.loads(proc.stdout)
                return info
            except Exception:
                return None
        return None
    except Exception:
        return None


class DownloadWorker(QThread):
    progress = pyqtSignal(dict)
    title_updated = pyqtSignal(str)
    finished = pyqtSignal(str, bool, list)
    error = pyqtSignal(str, str)

    def __init__(self, url, opts, parent=None):
        super().__init__(parent)
        self.url = url
        self.opts = opts
        self._is_cancelled = False

    def run(self):
        error_output = []
        stdout_lines = []
        try:
            # First, verify yt-dlp is available
            global _YT_DLP_PATH
            is_available, status_msg = check_yt_dlp_available()
            log.info(f"yt-dlp check: {status_msg}")
            if not is_available:
                error_msg = f"yt-dlp not available: {status_msg}"
                log.error(error_msg)
                self.error.emit(self.url, error_msg)
                return
            
            # Use the working path if we found one, otherwise use "yt-dlp" from PATH
            yt_dlp_cmd = _YT_DLP_PATH if _YT_DLP_PATH else shutil.which("yt-dlp") or "yt-dlp"
            # Attempt to extract metadata (title) first using yt-dlp JSON output so
            # the UI can display the actual video title immediately.
            try:
                meta_cmd = [yt_dlp_cmd, "--dump-single-json", self.url]
                meta_proc = subprocess.run(meta_cmd, capture_output=True, text=True, timeout=15, shell=False, stdin=subprocess.DEVNULL)
                if meta_proc.returncode == 0 and meta_proc.stdout:
                    try:
                        info = json.loads(meta_proc.stdout)
                        title = info.get("title") or info.get("id")
                        # store metadata for later thumbnail handling
                        self._meta_info = info
                        if title:
                            self.title_updated.emit(str(title))
                    except Exception:
                        pass
            except Exception:
                # Ignore metadata extraction failures and continue to download
                pass

            cmd = [yt_dlp_cmd] + (self.opts if self.opts else []) + [self.url]
            log.info(f"Running command: {' '.join(cmd)}")
            log.info(f"Command args: {cmd}")

            # If a temp directory was provided by the caller, snapshot its contents
            temp_snapshot = {}
            temp_dir = getattr(self, "temp_dir", None)
            # Cleanup any leftover preferred-thumb debug files from older runs
            try:
                if temp_dir and os.path.isdir(temp_dir):
                    for fn in os.listdir(temp_dir):
                        if fn.startswith("preferred_thumb_") and fn.lower().endswith(('.jpg', '.jpeg', '.png')):
                            try:
                                os.remove(os.path.join(temp_dir, fn))
                                log.info("Removed leftover debug thumbnail: %s", fn)
                            except Exception:
                                pass
            except Exception:
                pass
            if temp_dir and os.path.isdir(temp_dir):
                try:
                    for root, _, files in os.walk(temp_dir):
                        for f in files:
                            p = os.path.join(root, f)
                            try:
                                temp_snapshot[p] = os.path.getmtime(p)
                            except Exception:
                                temp_snapshot[p] = 0
                except Exception:
                    temp_snapshot = {}

            # Use separate pipes for stdout and stderr to capture all output
            # Explicitly set shell=False to avoid any shell interpretation issues
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,  # Separate stderr to capture errors
                universal_newlines=True,
                bufsize=1,
                errors='replace',  # Handle encoding errors gracefully
                shell=False,  # Don't use shell to avoid % character interpretation
                stdin=subprocess.DEVNULL # Prevent hanging if process reads stdin
            )

            # Use communicate() to reliably capture all output, especially if process exits quickly
            def read_stream(stream, is_stderr=False):
                """Read from a stream and populate the appropriate list."""
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
                                # Check if this looks like an error message
                                if any(keyword in line.lower() for keyword in ["error", "failed", "unable", "cannot", "invalid", "not found", "unavailable"]):
                                    error_output.append(line)
                                # Emit progress for non-error lines
                                if not any(keyword in line.lower() for keyword in ["error", "failed"]):
                                    self.progress.emit({"url": self.url, "text": line})
                                log.info(f"{self.url} stdout: {line}")
                except Exception as e:
                    log.warning(f"Error reading stream: {e}")
            
            # Read both streams in parallel using threads
            stdout_thread = threading.Thread(target=lambda: read_stream(process.stdout, False), daemon=True)
            stderr_thread = threading.Thread(target=lambda: read_stream(process.stderr, True), daemon=True)
            stdout_thread.start()
            stderr_thread.start()
            
            # Wait for process to complete
            return_code = process.wait()
            
            # Wait for threads to finish reading
            stdout_thread.join(timeout=2.0)
            stderr_thread.join(timeout=2.0)
            
            # If we still have no output, try communicate() as fallback
            if not stdout_lines and not error_output:
                try:
                    stdout_data, stderr_data = process.communicate(timeout=1)
                    if stdout_data:
                        for line in stdout_data.strip().split('\n'):
                            line = line.strip()
                            if line:
                                stdout_lines.append(line)
                                log.info(f"{self.url} stdout (fallback): {line}")
                    if stderr_data:
                        for line in stderr_data.strip().split('\n'):
                            line = line.strip()
                            if line:
                                error_output.append(line)
                                log.warning(f"{self.url} stderr (fallback): {line}")
                except Exception as e:
                    log.debug(f"Could not use communicate() fallback: {e}")
            # If cancellation was requested, treat as cancelled rather than an error
            try:
                if self._is_cancelled:
                    log.info(f"Download cancelled by user: {self.url}")
                    # Emit finished with success=False and no created files
                    self.finished.emit(self.url, False, [])
                    return
            except Exception:
                pass

            if return_code == 0:
                # Attempt to discover created files from stdout lines
                created_files = []
                try:
                    import re
                    for line in stdout_lines:
                        m = re.search(r"Destination:\s*(.+)$", line)
                        if m:
                            created_files.append(m.group(1).strip())
                        m2 = re.search(r"Merging formats into\s*(.+)$", line)
                        if m2:
                            created_files.append(m2.group(1).strip())
                except Exception:
                    pass

                # Also compare temp directory snapshot to find new/changed files
                try:
                    temp_dir = getattr(self, "temp_dir", None)
                    if temp_dir and os.path.isdir(temp_dir):
                        for root, _, files in os.walk(temp_dir):
                            for f in files:
                                p = os.path.join(root, f)
                                try:
                                    mtime = os.path.getmtime(p)
                                except Exception:
                                    mtime = 0
                                if p not in temp_snapshot or mtime > temp_snapshot.get(p, 0):
                                    created_files.append(p)
                except Exception:
                    pass

                # Sanitize, deduplicate and normalize created file paths
                def _sanitize(p):
                    try:
                        sp = str(p).strip()
                    except Exception:
                        return None
                    # strip surrounding quotes
                    if (sp.startswith('"') and sp.endswith('"')) or (sp.startswith("'") and sp.endswith("'")):
                        sp = sp[1:-1].strip()
                    sp = sp.strip()
                    if not sp:
                        return None
                    try:
                        return os.path.normpath(sp)
                    except Exception:
                        return sp

                seen = set()
                sanitized = []
                for p in created_files:
                    sp = _sanitize(p)
                    if not sp:
                        continue
                    if sp in seen:
                        continue
                    seen.add(sp)
                    sanitized.append(sp)
                created_files = sanitized
                # Attempt to download and embed preferred thumbnail based on metadata
                try:
                    self._handle_thumbnail_embedding(created_files)
                except Exception:
                    log.exception("Thumbnail handling failed")
                self.finished.emit(self.url, True, created_files)
            else:
                # Build comprehensive error message
                error_msg = f"yt-dlp error (code {return_code})"
                
                # If we have error output, use it
                if error_output:
                    # Get the most relevant error lines (usually the last ones)
                    relevant_errors = error_output[-5:] if len(error_output) > 5 else error_output
                    error_msg += f"\n\n{chr(10).join(relevant_errors)}"
                # Otherwise, check stdout for error-like messages
                elif stdout_lines:
                    # Look for error patterns in stdout
                    error_lines = [line for line in stdout_lines 
                                 if any(keyword in line.lower() for keyword in ["error", "failed", "unable", "cannot", "invalid", "not found", "permission", "access denied", "denied"])]
                    if error_lines:
                        error_msg += f"\n\n{chr(10).join(error_lines[-3:])}"
                    else:
                        # Last resort: show last few stdout lines
                        error_msg += f"\n\nLast output: {chr(10).join(stdout_lines[-3:])}"
                else:
                    error_msg += "\n\nNo error details captured. Check logs for more information."
                    # Check for common issues
                    if "invalid char" in str(stdout_lines).lower() or "invalid char" in str(error_output).lower():
                        error_msg += "\n\nPossible issues:"
                        error_msg += "\n- Invalid character in output path or template"
                        error_msg += "\n- Permission denied (check if output directory is writable)"
                        error_msg += "\n- yt-dlp version incompatibility"
                
                # Log full details
                log.error(f"Download failed for {self.url} with code {return_code}")
                log.error(f"Command: {' '.join(cmd)}")
                if error_output:
                    log.error(f"Error output: {chr(10).join(error_output)}")
                if stdout_lines:
                    log.error(f"Stdout output: {chr(10).join(stdout_lines[-10:])}")
                
                self.error.emit(self.url, error_msg)
        except FileNotFoundError:
            error_msg = "yt-dlp not found. Please install yt-dlp: pip install yt-dlp"
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
            try:
                info = getattr(self, "_meta_info", None)
                if not info:
                    log.debug("No metadata available for thumbnail handling")
                    return

                vid_id = info.get("id")
                thumbs = info.get("thumbnails") or []
                thumb_url = None
                # choose largest thumbnail by area
                best_area = 0
                for t in thumbs:
                    url = t.get("url") if isinstance(t, dict) else None
                    if not url:
                        continue
                    w = t.get("width") or 0
                    h = t.get("height") or 0
                    area = (w or 0) * (h or 0)
                    if area > best_area:
                        best_area = area
                        thumb_url = url
                if not thumb_url:
                    thumb_url = info.get("thumbnail")

                if not thumb_url:
                    log.debug("No thumbnail URL found in metadata for %s", vid_id)
                    return

                # build candidate variants (YouTube-friendly)
                candidates = [(thumb_url, "original")]
                try:
                    if "hqdefault" in thumb_url:
                        candidates.append((thumb_url.replace("hqdefault", "maxresdefault"), "maxresdefault"))
                    if "default" in thumb_url:
                        candidates.append((thumb_url.replace("default", "maxresdefault"), "maxresdefault"))
                    if "sddefault" in thumb_url:
                        candidates.append((thumb_url.replace("sddefault", "maxresdefault"), "maxresdefault"))
                except Exception:
                    pass

                headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"}
                # directory to store temporary thumbnail while embedding
                temp_dir = getattr(self, "temp_dir", None)
                download_dir = temp_dir or os.getcwd()
                os.makedirs(download_dir, exist_ok=True)

                downloaded_thumb = None
                method = None
                created_temp_files = []
                for cand, label in candidates:
                    try:
                        log.info("Trying thumbnail candidate: %s", cand)
                        head = requests.head(cand, headers=headers, timeout=8)
                        ok = head.status_code == 200
                        r = None
                        if not ok:
                            r = requests.get(cand, stream=True, headers=headers, timeout=12)
                            ok = r.status_code == 200
                        else:
                            r = requests.get(cand, stream=True, headers=headers, timeout=12)
                        if ok and r is not None and r.status_code == 200:
                            # write to a temporary file so we don't leave debug artifacts
                            with tempfile.NamedTemporaryFile(delete=False, suffix='.jpg', dir=download_dir) as tf:
                                for chunk in r.iter_content(1024 * 8):
                                    if chunk:
                                        tf.write(chunk)
                                out_path = tf.name
                            downloaded_thumb = out_path
                            created_temp_files.append(out_path)
                            method = label
                            log.info("Downloaded thumbnail %s", out_path)
                            break
                    except Exception as e:
                        log.debug("Thumbnail candidate failed: %s (%s)", cand, e)

                if not downloaded_thumb:
                    log.debug("No thumbnail could be downloaded for %s", vid_id)
                    return

                # select target media file among created_files (prefer existing files containing id)
                targets = []
                for p in created_files:
                    try:
                        sp = str(p).strip().strip('"').strip("'")
                        sp = os.path.normpath(sp)
                        if os.path.exists(sp):
                            # prefer files with the id in the basename
                            targets.append(sp)
                    except Exception:
                        continue

                # prefer targets that contain the vid_id in the filename
                id_targets = [t for t in targets if vid_id and vid_id in os.path.basename(t)]
                if id_targets:
                    targets = id_targets

                # fallback: search temp and completed dirs for a file containing the id
                if not targets:
                    try:
                        search_dirs = []
                        if temp_dir and os.path.isdir(temp_dir):
                            search_dirs.append(temp_dir)
                        if completed_dir and os.path.isdir(completed_dir):
                            search_dirs.append(completed_dir)
                        for d in search_dirs:
                            for root, _, files in os.walk(d):
                                for fn in files:
                                    if vid_id and vid_id in fn:
                                        targets.append(os.path.join(root, fn))
                    except Exception:
                        log.exception("Error searching for target media files in temp/completed dirs")

                if not targets:
                    log.debug("No target media file to embed thumbnail into for %s", vid_id)
                    # cleanup temp thumbnail
                    for p in created_temp_files:
                        try:
                            if os.path.exists(p):
                                os.remove(p)
                        except Exception:
                            pass
                    return

                targets = sorted(targets, key=lambda p: os.path.getmtime(p), reverse=True)
                target = targets[0]
                log.info("Embedding thumbnail into %s", target)

                # ensure jpg
                thumb_jpg = downloaded_thumb
                conv = None
                if not thumb_jpg.lower().endswith((".jpg", ".jpeg")):
                    conv = os.path.join(download_dir, f"embed_thumb_{vid_id}.jpg")
                    try:
                        subprocess.run(["ffmpeg", "-y", "-i", downloaded_thumb, conv], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                        thumb_jpg = conv
                        created_temp_files.append(conv)
                    except Exception:
                        log.exception("Failed to convert thumbnail to jpg; will try using original")

                # create an output temp filename that preserves the original extension
                root, ext = os.path.splitext(target)
                if not ext:
                    ext = ""
                out_tmp = f"{root}.embedtmp{ext}"

                def _ffprobe(path):
                    try:
                        p = subprocess.run(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-print_format", "json", path], capture_output=True, text=True, check=False)
                        return p.stdout, p.stderr, p.returncode
                    except Exception as e:
                        return None, str(e), 1

                # ffprobe info for target before embedding
                try:
                    info_out, info_err, info_rc = _ffprobe(target)
                except Exception:
                    log.exception("ffprobe failed on target before embedding")

                cmd = [
                    "ffmpeg",
                    "-y",
                    "-i",
                    target,
                    "-i",
                    thumb_jpg,
                    "-map",
                    "0",
                    "-map",
                    "1",
                    "-c",
                    "copy",
                    "-metadata:s:v:1",
                    "title=Album cover",
                    "-metadata:s:v:1",
                    "comment=Cover (front)",
                    "-disposition:v:1",
                    "attached_pic",
                    out_tmp,
                ]

                try:
                    res = subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    # replace original
                    try:
                        os.replace(out_tmp, target)
                        log.info("Embedded thumbnail into %s", target)
                        # ffprobe after
                        info_out2, info_err2, _ = _ffprobe(target)
                    except Exception:
                        log.exception("Failed to replace original file with embedded output")
                    finally:
                        # cleanup any temp thumbnail files
                        for p in created_temp_files:
                            try:
                                if os.path.exists(p):
                                    os.remove(p)
                            except Exception:
                                pass
                        try:
                            if os.path.exists(out_tmp):
                                os.remove(out_tmp)
                        except Exception:
                            pass
                except subprocess.CalledProcessError as e:
                    stderr = e.stderr.decode(errors='replace') if isinstance(e.stderr, (bytes, bytearray)) else str(e.stderr)
                    stdout = e.stdout.decode(errors='replace') if isinstance(e.stdout, (bytes, bytearray)) else str(e.stdout)
                    log.error("ffmpeg embedding failed for %s: %s", target, stderr or stdout)
                    # cleanup any temp thumbnail files
                    for p in created_temp_files:
                        try:
                            if os.path.exists(p):
                                os.remove(p)
                        except Exception:
                            pass
                    try:
                        if os.path.exists(out_tmp):
                            os.remove(out_tmp)
                    except Exception:
                        pass
                    return
            except Exception:
                log.exception("Error in thumbnail embedding helper")
