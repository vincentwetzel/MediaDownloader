import logging
import subprocess
import json
import shutil
import sys
from yt_dlp.utils import DownloadError
from core.yt_dlp_worker import _YT_DLP_PATH

# --- HIDE CONSOLE WINDOW for SUBPROCESS ---
creation_flags = 0
if sys.platform == "win32" and getattr(sys, "frozen", False):
    creation_flags = subprocess.CREATE_NO_WINDOW

log = logging.getLogger(__name__)


class PlaylistExpansionError(Exception):
    """Custom exception for errors during playlist expansion."""
    pass


def _entry_to_url(entry):
    """Convert a yt-dlp playlist entry to a usable URL string.
    Prefer `webpage_url`, then `url`, then construct a YouTube watch URL from `id`.
    """
    try:
        if not entry:
            return None
        if isinstance(entry, dict):
            if entry.get("webpage_url"):
                return entry.get("webpage_url")
            if entry.get("url"):
                return entry.get("url")
            vid = entry.get("id")
            if vid:
                return f"https://www.youtube.com/watch?v={vid}"
        # Fallback: if it's a plain string
        if isinstance(entry, str):
            return entry
    except Exception:
        pass
    return None


def expand_playlist(url):
    """Return list of URLs if the input is a playlist, otherwise [url].

    Uses yt-dlp as a subprocess to offload playlist processing and avoid
    blocking the GUI thread.
    """
    try:
        yt_dlp_cmd = _YT_DLP_PATH if _YT_DLP_PATH else shutil.which("yt-dlp") or "yt-dlp"
        
        # Use --dump-single-json and --flat-playlist for efficient metadata extraction
        cmd = [
            yt_dlp_cmd,
            url,
            "--dump-single-json",
            "--flat-playlist",
            "--quiet"
        ]

        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60,  # 60-second timeout for playlist fetching
            shell=False,
            stdin=subprocess.DEVNULL,
            errors='replace',
            creationflags=creation_flags
        )

        if proc.returncode != 0:
            # Check for specific, actionable errors like premieres
            if "premiere" in proc.stderr.lower():
                raise PlaylistExpansionError("This video is a premiere and cannot be downloaded yet.")
            log.error(f"yt-dlp failed to expand playlist {url}. Stderr: {proc.stderr}")
            raise PlaylistExpansionError(f"Failed to process URL/playlist. See logs for details.")

        if proc.stdout:
            try:
                info = json.loads(proc.stdout)
                entries = info.get("entries") if isinstance(info, dict) else None
                if entries:
                    urls = [_entry_to_url(e) for e in entries if _entry_to_url(e)]
                    if urls:
                        log.debug(f"Expanded playlist {url} -> {len(urls)} items")
                        return urls
            except json.JSONDecodeError:
                log.error(f"Failed to parse JSON from yt-dlp for playlist: {url}")
                raise PlaylistExpansionError("Failed to parse playlist data.")

        # Fallback to returning the original URL if no entries found
        return [url]

    except subprocess.TimeoutExpired:
        log.error(f"yt-dlp timed out while expanding playlist: {url}")
        raise PlaylistExpansionError("Playlist expansion timed out. The playlist might be very large or the service is slow.")
    except DownloadError as e:
        # This might be caught by the subprocess check, but as a fallback
        if "premiere" in str(e).lower():
            raise PlaylistExpansionError("This video is a premiere and cannot be downloaded yet.")
        log.error(f"Failed to expand playlist {url}: {e}")
        raise PlaylistExpansionError(f"Failed to process URL/playlist: {e}")
    except Exception as e:
        log.error(f"An unexpected error occurred while expanding playlist {url}: {e}")
        raise PlaylistExpansionError(f"An unexpected error occurred: {e}")


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
