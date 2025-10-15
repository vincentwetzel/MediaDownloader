import logging
import yt_dlp

log = logging.getLogger(__name__)


def expand_playlist(url):
    """Return list of URLs if the input is a playlist, otherwise [url]."""
    try:
        ydl_opts = {"quiet": True, "extract_flat": True}
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            info = ydl.extract_info(url, download=False)
            if "entries" in info:
                urls = [entry["url"] for entry in info["entries"]]
                log.debug(f"Expanded playlist {url} -> {len(urls)} items")
                return urls
            return [url]
    except Exception as e:
        log.error(f"Failed to expand playlist {url}: {e}")
        return [url]
