import logging
import subprocess
import json
import shutil
import sys
from core.binary_manager import get_binary_path

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


def expand_playlist(url):
    """Return list of URLs if the input is a playlist, otherwise [url].

    Uses yt-dlp as a subprocess to offload playlist processing and avoid
    blocking the GUI thread.
    """
    try:
        yt_dlp_cmd = get_binary_path("yt-dlp")
        
        if not yt_dlp_cmd:
            log.error("yt-dlp binary not found for playlist expansion.")
            raise PlaylistExpansionError("yt-dlp binary not found.")
        
        # Use --dump-single-json and --flat-playlist for efficient metadata extraction
        # Added --no-cache-dir and --no-write-playlist-metafiles
        cmd = [
            yt_dlp_cmd,
            "--dump-single-json",
            "--flat-playlist",
            "--no-cache-dir",
            "--no-write-playlist-metafiles",
            "--no-input",
            "--quiet",
            url
        ]

        log.debug(f"Expanding playlist with command: {cmd}")
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
            # If it fails, just return the original URL - maybe it's not a playlist or yt-dlp can handle it directly
            return [url]

        if proc.stdout:
            try:
                info = json.loads(proc.stdout)
                
                # If it's a playlist, it should have 'entries'
                if info.get('_type') == 'playlist' or 'entries' in info:
                    entries = info.get("entries")
                    if entries:
                        urls = []
                        for e in entries:
                            u = _entry_to_url(e)
                            if u:
                                urls.append(u)
                        
                        if urls:
                            log.info(f"Expanded playlist {url} -> {len(urls)} items")
                            return urls
                
                # If not a playlist or no entries found, return original URL
                return [url]
                
            except json.JSONDecodeError:
                log.error(f"Failed to parse JSON from yt-dlp for playlist: {url}")
                return [url]

        # Fallback to returning the original URL if no stdout
        return [url]

    except subprocess.TimeoutExpired:
        log.error(f"yt-dlp timed out while expanding playlist: {url}")
        raise PlaylistExpansionError("Playlist expansion timed out. The playlist might be very large or the service is slow.")
    except Exception as e:
        log.error(f"An unexpected error occurred while expanding playlist {url}: {e}")
        # Return original URL on error so we at least try to download it
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
