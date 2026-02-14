import logging
import subprocess
import json
import shutil
import sys
import re
import os
import time
import threading
from urllib.parse import urlparse, parse_qs
from core.binary_manager import get_binary_path
from core.yt_dlp_args_builder import build_yt_dlp_args

# --- HIDE CONSOLE WINDOW for SUBPROCESS ---
creation_flags = 0
if sys.platform == "win32" and getattr(sys, "frozen", False):
    creation_flags = subprocess.CREATE_NO_WINDOW

log = logging.getLogger(__name__)


class PlaylistExpansionError(Exception):
    """Custom exception for errors during playlist expansion."""
    pass


def _append_auth_runtime_args(cmd, config_manager):
    """Append auth/runtime flags used by normal yt-dlp downloads."""
    if not config_manager:
        return

    try:
        cookies_browser = config_manager.get("General", "cookies_from_browser", fallback="None")
        if cookies_browser and cookies_browser != "None":
            cmd.extend(["--cookies-from-browser", cookies_browser])
    except Exception:
        pass

    try:
        js_runtime_path = config_manager.get("General", "js_runtime_path", fallback="")
        if not js_runtime_path or not os.path.exists(js_runtime_path):
            js_runtime_path = get_binary_path("deno")
        if js_runtime_path and os.path.exists(js_runtime_path):
            runtime_name = os.path.basename(js_runtime_path).split(".")[0]
            cmd.extend(["--js-runtimes", f"{runtime_name}:{js_runtime_path}"])
    except Exception:
        pass


def _dedupe_keep_order(items):
    seen = set()
    out = []
    for item in items:
        if not item:
            continue
        if item in seen:
            continue
        seen.add(item)
        out.append(item)
    return out


def _dedupe_entries_keep_order(entries):
    seen = set()
    out = []
    for entry in entries or []:
        if not isinstance(entry, dict):
            continue
        u = (entry.get("url") or "").strip()
        if not u or u in seen:
            continue
        seen.add(u)
        out.append({"url": u, "title": (entry.get("title") or "").strip()})
    return out


def _parse_print_line(line: str):
    """Parse a yt-dlp --print line into (title, url, id)."""
    raw = (line or "").strip()
    if not raw:
        return "", "", ""

    # Preferred tab-separated format.
    if "\t" in raw:
        parts = raw.split("\t")
        if len(parts) >= 3:
            title = parts[0].strip()
            url = parts[1].strip()
            vid = parts[2].strip()
            return title, url, vid

    # Fallback: find first URL and treat prefix as title.
    m = re.search(r"https?://\S+", raw)
    if m:
        u = m.group(0).strip()
        title = raw[:m.start()].strip(" -\t")
        # Best-effort id extraction from common YouTube URL forms.
        vid = ""
        try:
            pu = urlparse(u)
            q = parse_qs(pu.query)
            vid = (q.get("v") or [None])[0] or ""
            if not vid:
                path_last = (pu.path or "").rstrip("/").split("/")[-1]
                if re.match(r"^[A-Za-z0-9_-]{11}$", path_last):
                    vid = path_last
        except Exception:
            pass
        return title, u, vid

    return "", "", ""


def _entry_to_url(entry):
    """Convert a yt-dlp playlist entry to a usable URL string.
    Prefer `webpage_url`, then `url`, then construct a YouTube watch URL from `id`.
    """
    try:
        if not entry:
            return None
        if isinstance(entry, dict):
            vid = entry.get("id")
            ie_key = (entry.get("ie_key") or entry.get("extractor_key") or "").lower()

            # Prefer a full webpage URL when available.
            # If it is a YouTube watch URL, drop playlist/query extras so each
            # entry is treated as a single-video URL.
            webpage_url = entry.get("webpage_url")
            if isinstance(webpage_url, str) and webpage_url.startswith(("http://", "https://")):
                try:
                    pu = urlparse(webpage_url)
                    host = (pu.hostname or "").lower()
                    if "youtube.com" in host and pu.path == "/watch":
                        q = parse_qs(pu.query)
                        v = (q.get("v") or [None])[0]
                        if v:
                            return f"https://www.youtube.com/watch?v={v}"
                except Exception:
                    pass
                return webpage_url

            # In flat-playlist mode, `url` is often just the video ID for YouTube.
            # Only accept it directly if it already looks like a URL.
            url_val = entry.get("url")
            if isinstance(url_val, str):
                if url_val.startswith(("http://", "https://")):
                    return url_val
                if len(url_val) == 11 and ("youtube" in ie_key or ie_key in ("youtube", "youtubetab")):
                    return f"https://www.youtube.com/watch?v={url_val}"

            if vid:
                # Check if it's a youtube video ID (11 chars usually)
                if len(vid) == 11:
                    return f"https://www.youtube.com/watch?v={vid}"
                return vid # Return ID as fallback
        # Fallback: if it's a plain string
        if isinstance(entry, str):
            return entry
    except Exception:
        pass
    return None


def _entry_to_title(entry):
    try:
        if isinstance(entry, dict):
            t = entry.get("title")
            if isinstance(t, str) and t.strip():
                return t.strip()
    except Exception:
        pass
    return ""


def _emit_progress(progress_callback, payload):
    if not progress_callback:
        return
    try:
        progress_callback(payload)
    except Exception:
        log.debug("Playlist progress callback failed", exc_info=True)


def _parse_stream_print_line(line: str):
    """Parse stream print line into an entry + index/count metadata."""
    raw = (line or "").strip()
    if not raw:
        return None, 0, 0

    parts = raw.split("\t")
    if len(parts) < 6:
        return None, 0, 0

    idx_raw = (parts[0] or "").strip()
    count_raw = (parts[1] or "").strip()
    title = (parts[2] or "").strip()
    webpage_url = (parts[3] or "").strip()
    url_val = (parts[4] or "").strip()
    vid = (parts[5] or "").strip()

    index = int(idx_raw) if idx_raw.isdigit() else 0
    total = int(count_raw) if count_raw.isdigit() else 0

    chosen = None
    for cand in (webpage_url, url_val):
        if isinstance(cand, str) and cand.startswith(("http://", "https://")) and cand.upper() != "NA":
            chosen = cand
            break

    if not chosen and vid and re.match(r"^[A-Za-z0-9_-]{11}$", vid):
        chosen = f"https://www.youtube.com/watch?v={vid}"

    if not chosen:
        return None, index, total

    return {"url": chosen, "title": title}, index, total


def _parse_playlist_status_line(line: str):
    """Parse yt-dlp stderr status lines for extraction progress."""
    raw = (line or "").strip()
    if not raw:
        return None

    m = re.search(r"Downloading item\s+(\d+)\s+of\s+(\d+)", raw, flags=re.IGNORECASE)
    if m:
        return {
            "phase": "extracting",
            "current": int(m.group(1)),
            "total": int(m.group(2)),
            "status_text": raw
        }

    m = re.search(r"Extracting URL:\s*(\S+)", raw, flags=re.IGNORECASE)
    if m:
        return {
            "phase": "extracting",
            "url": m.group(1).strip(),
            "status_text": raw
        }

    return None


def _expand_playlist_via_print(url, yt_dlp_cmd, config_manager=None, progress_callback=None):
    """Expansion that parses line-based --print output and streams progress."""
    cmd = [
        yt_dlp_cmd,
        "--flat-playlist",
        "--lazy-playlist",
        "--ignore-errors",
        "--no-cache-dir",
        "--no-write-playlist-metafiles",
        "--no-input",
        "--print",
        "%(playlist_index)s\t%(playlist_count)s\t%(title)s\t%(webpage_url)s\t%(url)s\t%(id)s",
        url,
    ]
    _append_auth_runtime_args(cmd, config_manager)

    log.debug(f"Expanding playlist via --print fallback: {cmd}")
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        shell=False,
        stdin=subprocess.DEVNULL,
        errors='replace',
        creationflags=creation_flags
    )

    deadline = time.monotonic() + 60
    entries = []
    total_seen = 0
    status_state = {"current": 0, "total": 0}

    def _stderr_reader():
        try:
            if not proc.stderr:
                return
            for err_line in proc.stderr:
                payload = _parse_playlist_status_line(err_line)
                if not payload:
                    continue
                cur = payload.get("current") or 0
                tot = payload.get("total") or 0
                if cur:
                    status_state["current"] = max(status_state["current"], int(cur))
                if tot:
                    status_state["total"] = max(status_state["total"], int(tot))
                _emit_progress(progress_callback, payload)
        except Exception:
            log.debug("stderr reader failed for playlist expansion", exc_info=True)

    stderr_thread = threading.Thread(target=_stderr_reader, daemon=True)
    stderr_thread.start()

    while True:
        if time.monotonic() > deadline:
            try:
                proc.kill()
            except Exception:
                pass
            raise PlaylistExpansionError("Playlist expansion timed out. The playlist might be very large or the service is slow.")

        raw = proc.stdout.readline() if proc.stdout else ""
        if raw:
            entry, idx, total = _parse_stream_print_line(raw)
            if total > 0:
                total_seen = total
            if entry:
                entries.append(entry)
                current_num = idx or len(entries)
                known_total = total_seen or status_state.get("total") or 0
                _emit_progress(progress_callback, {
                    "phase": "extracting",
                    "url": entry.get("url", ""),
                    "title": entry.get("title", ""),
                    "current": current_num,
                    "total": known_total
                })
            continue

        if proc.poll() is not None:
            break
        time.sleep(0.05)

    entries = _dedupe_entries_keep_order(entries)
    stderr_text = ""
    try:
        stderr_text = proc.stderr.read() if proc.stderr else ""
    except Exception:
        stderr_text = ""
    if entries:
        log.info(f"Expanded playlist {url} via fallback -> {len(entries)} items")
    else:
        log.warning(
            "Print fallback produced no entries for %s (rc=%s, stderr_len=%s)",
            url,
            proc.returncode,
            len(stderr_text or "")
        )
    return entries


def _expand_playlist_via_full_print(url, yt_dlp_cmd, config_manager=None, progress_callback=None):
    """Fallback expansion using full extractor path (no --flat-playlist)."""
    cmd = [
        yt_dlp_cmd,
        "--ignore-errors",
        "--no-cache-dir",
        "--no-write-playlist-metafiles",
        "--no-input",
        "--quiet",
        "--no-download",
        "--yes-playlist",
        "--print",
        "%(title)s\t%(webpage_url)s\t%(id)s",
        url,
    ]
    _append_auth_runtime_args(cmd, config_manager)
    log.debug(f"Expanding playlist via full --print fallback: {cmd}")

    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=120,
        shell=False,
        stdin=subprocess.DEVNULL,
        errors='replace',
        creationflags=creation_flags
    )

    entries = []
    for raw in (proc.stdout or "").splitlines():
        line = (raw or "").strip()
        if not line:
            continue
        parts = line.split("\t")
        title = parts[0].strip() if len(parts) > 0 else ""
        webpage_url = parts[1].strip() if len(parts) > 1 else ""
        vid = parts[2].strip() if len(parts) > 2 else ""

        chosen = None
        if webpage_url.startswith(("http://", "https://")) and webpage_url.upper() != "NA":
            chosen = webpage_url
        elif vid and re.match(r"^[A-Za-z0-9_-]{11}$", vid):
            chosen = f"https://www.youtube.com/watch?v={vid}"

        if chosen:
            entries.append({"url": chosen, "title": title})

    entries = _dedupe_entries_keep_order(entries)
    if entries:
        log.info(f"Expanded playlist {url} via full fallback -> {len(entries)} items")
    return entries


def _expand_playlist_via_lazy_print(url, yt_dlp_cmd, config_manager=None, progress_callback=None):
    """Fallback expansion using lazy playlist traversal."""
    cmd = [
        yt_dlp_cmd,
        "--ignore-errors",
        "--no-cache-dir",
        "--no-write-playlist-metafiles",
        "--no-input",
        "--quiet",
        "--yes-playlist",
        "--lazy-playlist",
        "--print",
        "%(title)s\t%(webpage_url)s\t%(id)s",
        url,
    ]
    _append_auth_runtime_args(cmd, config_manager)
    log.debug(f"Expanding playlist via lazy --print fallback: {cmd}")

    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=120,
        shell=False,
        stdin=subprocess.DEVNULL,
        errors='replace',
        creationflags=creation_flags
    )

    entries = []
    for raw in (proc.stdout or "").splitlines():
        line = (raw or "").strip()
        if not line:
            continue
        parts = line.split("\t")
        if len(parts) < 3:
            continue
        title = parts[0].strip()
        webpage_url = parts[1].strip()
        vid = parts[2].strip()

        chosen = None
        if webpage_url.startswith(("http://", "https://")) and webpage_url.upper() != "NA":
            chosen = webpage_url
        elif vid and re.match(r"^[A-Za-z0-9_-]{11}$", vid):
            chosen = f"https://www.youtube.com/watch?v={vid}"

        if chosen:
            entries.append({"url": chosen, "title": title})

    entries = _dedupe_entries_keep_order(entries)
    if entries:
        log.info(f"Expanded playlist {url} via lazy fallback -> {len(entries)} items")
    return entries


def _expand_playlist_via_runtime_args(url, yt_dlp_cmd, config_manager=None, opts=None, progress_callback=None):
    """Final fallback: run yt-dlp with worker-like args and print playlist entries."""
    cmd = [yt_dlp_cmd]
    try:
        runtime_opts = dict(opts or {})
        # Force playlist expansion mode for preflight.
        runtime_opts["playlist_mode"] = "all"
        runtime_args = build_yt_dlp_args(runtime_opts, config_manager) if config_manager else []
    except Exception:
        runtime_args = []

    # Strip output template args for preflight enumeration.
    filtered_args = []
    i = 0
    while i < len(runtime_args):
        arg = runtime_args[i]
        if arg in ("-o", "-P", "--paths"):
            i += 2
            continue
        filtered_args.append(arg)
        i += 1

    # Ensure this command only enumerates entries.
    cmd.extend(filtered_args)
    cmd.extend([
        "--ignore-errors",
        "--skip-download",
        "--yes-playlist",
        "--print",
        "%(title)s\t%(webpage_url)s\t%(id)s",
        url,
    ])

    log.debug(f"Expanding playlist via runtime-args fallback: {cmd}")
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        shell=False,
        stdin=subprocess.DEVNULL,
        errors='replace',
        creationflags=creation_flags
    )

    deadline = time.monotonic() + 180
    entries = []
    while True:
        if time.monotonic() > deadline:
            try:
                proc.kill()
            except Exception:
                pass
            raise PlaylistExpansionError("Playlist expansion timed out. The playlist might be very large or the service is slow.")

        raw = proc.stdout.readline() if proc.stdout else ""
        if raw:
            line = (raw or "").strip()
            if not line:
                continue
            if line.startswith("WARNING:") or line.startswith("ERROR:"):
                continue

            title, webpage_url, vid = _parse_print_line(line)

            chosen = None
            if webpage_url.startswith(("http://", "https://")) and webpage_url.upper() != "NA":
                chosen = webpage_url
            elif vid and re.match(r"^[A-Za-z0-9_-]{11}$", vid):
                chosen = f"https://www.youtube.com/watch?v={vid}"

            if chosen:
                entries.append({"url": chosen, "title": title})
                _emit_progress(progress_callback, {
                    "phase": "extracting",
                    "url": chosen,
                    "title": title,
                    "current": len(entries),
                    "total": 0,
                    "status_text": f"Gathering playlist URLs... found {len(entries)}"
                })
            continue

        if proc.poll() is not None:
            break
        time.sleep(0.05)

    entries = _dedupe_entries_keep_order(entries)
    stderr_text = ""
    try:
        stderr_text = proc.stderr.read() if proc.stderr else ""
    except Exception:
        stderr_text = ""
    if entries:
        log.info(f"Expanded playlist {url} via runtime fallback -> {len(entries)} items")
    else:
        # Keep this visible so expansion failures are diagnosable.
        log.warning(
            "Runtime fallback produced no entries for %s (rc=%s, stdout_lines=%s, stderr_len=%s)",
            url,
            proc.returncode,
            0,
            len(stderr_text or "")
        )
    return entries


def expand_playlist_entries(url, config_manager=None, opts=None, progress_callback=None):
    """Return list of playlist entries as {'url': str, 'title': str}.

    If input cannot be expanded as a playlist, returns a single entry for the URL.
    """
    """Return list of URLs if the input is a playlist, otherwise [url].

    Uses yt-dlp as a subprocess to offload playlist processing and avoid
    blocking the GUI thread.
    """
    try:
        yt_dlp_cmd = get_binary_path("yt-dlp")

        if not yt_dlp_cmd:
            log.error("yt-dlp binary not found for playlist expansion.")
            raise PlaylistExpansionError("yt-dlp binary not found.")

        # Use line-based extraction first so the UI can show per-item progress.
        try:
            streamed_entries = _expand_playlist_via_print(
                url,
                yt_dlp_cmd,
                config_manager=config_manager,
                progress_callback=progress_callback
            )
            if streamed_entries:
                return streamed_entries
        except Exception:
            log.debug("Streamed playlist expansion failed", exc_info=True)

        # JSON fallback for providers where print mode does not yield entries.
        cmd = [
            yt_dlp_cmd,
            "--dump-single-json",
            "--flat-playlist",
            "--ignore-errors",
            "--no-cache-dir",
            "--no-write-playlist-metafiles",
            "--no-input",
            "--quiet",
            url
        ]
        _append_auth_runtime_args(cmd, config_manager)

        log.debug(f"Expanding playlist with command: {cmd}")
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60,
            shell=False,
            stdin=subprocess.DEVNULL,
            errors='replace',
            creationflags=creation_flags
        )

        parsed_json_ok = False
        if proc.stdout:
            try:
                info = json.loads(proc.stdout)
                parsed_json_ok = True

                if info.get('_type') == 'playlist' or 'entries' in info:
                    entries = info.get("entries")
                    if entries:
                        out_entries = []
                        total_entries = len(entries)
                        for i, e in enumerate(entries, start=1):
                            u = _entry_to_url(e)
                            if u:
                                title = _entry_to_title(e)
                                out_entries.append({"url": u, "title": title})
                                _emit_progress(progress_callback, {
                                    "phase": "extracting",
                                    "url": u,
                                    "title": title,
                                    "current": i,
                                    "total": total_entries
                                })

                        out_entries = _dedupe_entries_keep_order(out_entries)
                        if out_entries:
                            log.info(f"Expanded playlist {url} -> {len(out_entries)} items")
                            return out_entries

            except json.JSONDecodeError:
                log.error(f"Failed to parse JSON from yt-dlp for playlist: {url}")

        try:
            full_entries = _expand_playlist_via_full_print(
                url,
                yt_dlp_cmd,
                config_manager=config_manager,
                progress_callback=progress_callback
            )
            if full_entries:
                return full_entries
        except Exception:
            log.debug("Full fallback playlist expansion failed", exc_info=True)

        try:
            lazy_entries = _expand_playlist_via_lazy_print(
                url,
                yt_dlp_cmd,
                config_manager=config_manager,
                progress_callback=progress_callback
            )
            if lazy_entries:
                return lazy_entries
        except Exception:
            log.debug("Lazy fallback playlist expansion failed", exc_info=True)

        try:
            runtime_entries = _expand_playlist_via_runtime_args(
                url,
                yt_dlp_cmd,
                config_manager=config_manager,
                opts=opts,
                progress_callback=progress_callback
            )
            if runtime_entries:
                return runtime_entries
        except Exception:
            log.debug("Runtime-args fallback playlist expansion failed", exc_info=True)

        if proc.returncode != 0:
            if "premiere" in (proc.stderr or "").lower():
                raise PlaylistExpansionError("This video is a premiere and cannot be downloaded yet.")
            log.error(f"yt-dlp failed to expand playlist {url}. Stderr: {proc.stderr}")

            try:
                parsed = urlparse(url)
                query = parse_qs(parsed.query)
                list_id = (query.get("list") or [None])[0]
                if list_id:
                    retry_url = f"https://www.youtube.com/playlist?list={list_id}"
                    if retry_url != url:
                        log.info(f"Retrying playlist expansion via canonical URL: {retry_url}")
                        return expand_playlist_entries(
                            retry_url,
                            config_manager=config_manager,
                            opts=opts,
                            progress_callback=progress_callback
                        )
            except Exception:
                pass

        if parsed_json_ok:
            return [{"url": url, "title": ""}]

        return [{"url": url, "title": ""}]

    except subprocess.TimeoutExpired:
        log.error(f"yt-dlp timed out while expanding playlist: {url}")
        raise PlaylistExpansionError("Playlist expansion timed out. The playlist might be very large or the service is slow.")
    except Exception as e:
        log.error(f"An unexpected error occurred while expanding playlist {url}: {e}")
        # Return original URL on error so we at least try to download it
        return [{"url": url, "title": ""}]


def expand_playlist(url, config_manager=None, opts=None):
    """Backward-compatible URL-only expansion API."""
    try:
        entries = expand_playlist_entries(url, config_manager=config_manager, opts=opts)
        urls = _dedupe_keep_order([e.get("url") for e in entries if isinstance(e, dict)])
        return urls or [url]
    except Exception:
        return [url]


def is_likely_playlist(url: str) -> bool:
    """Quick heuristic to detect whether a URL is likely a playlist URL.

    This is intentionally lightweight and checks common patterns so the UI
    can respond immediately without waiting for full yt-dlp extraction.
    """
    try:
        if not url or not isinstance(url, str):
            return False
        low = url.lower()
        # Query param 'list=' is the most common indicator (YouTube, Music, etc.)
        if "list=" in low:
            return True
        # Common playlist paths
        if "/playlist" in low or "/playlists" in low:
            return True
        # Some sites use 'set=' or 'album=' for grouped resources
        if "set=" in low or "album=" in low:
            return True
        # Vimeo/other providers may include 'channel' or 'series'
        if "series" in low or "channel" in low:
            return True
        return False
    except Exception:
        return False
