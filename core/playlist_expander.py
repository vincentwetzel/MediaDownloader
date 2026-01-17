import logging
import yt_dlp

log = logging.getLogger(__name__)


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

    Tries a couple of strategies to detect and expand playlists reliably
    (some services like YouTube Music may redirect). Returns the original
    URL if expansion cannot be determined.
    """
    try:
        # Try with default extraction (not flat) first to get full info
        ydl_opts = {"quiet": True}
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            info = ydl.extract_info(url, download=False)
            # If yt-dlp reports entries, convert them to URLs
            entries = info.get("entries") if isinstance(info, dict) else None
            if entries:
                urls = []
                for entry in entries:
                    u = _entry_to_url(entry)
                    if u:
                        urls.append(u)
                if urls:
                    log.debug(f"Expanded playlist {url} -> {len(urls)} items")
                    return urls

        # Fallback: try extract_flat which may behave differently for some sites
        try:
            ydl_opts = {"quiet": True, "extract_flat": True}
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(url, download=False)
                entries = info.get("entries") if isinstance(info, dict) else None
                if entries:
                    urls = []
                    for entry in entries:
                        u = _entry_to_url(entry)
                        if u:
                            urls.append(u)
                    if urls:
                        log.debug(f"Expanded (flat) playlist {url} -> {len(urls)} items")
                        return urls
        except Exception:
            pass

        return [url]
    except Exception as e:
        log.error(f"Failed to expand playlist {url}: {e}")
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
