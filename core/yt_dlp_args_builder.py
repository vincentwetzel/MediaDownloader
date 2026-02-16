import os
import logging
import re
from core.binary_manager import get_binary_path

log = logging.getLogger(__name__)


def _normalize_rate_limit(value):
    """Return a yt-dlp compatible rate-limit string, or empty string for unlimited/invalid."""
    if value is None:
        return ""

    normalized = str(value).strip()
    if not normalized:
        return ""

    if normalized.lower() in ("0", "none", "no limit", "unlimited"):
        return ""

    # Accept values like "250K", "2M", "10M", "1G".
    if re.fullmatch(r"[1-9]\d*[KMGkmg]?", normalized):
        return normalized.upper()

    log.warning("Ignoring invalid rate_limit setting: %r", value)
    return ""

def build_yt_dlp_args(opts, config_manager):
    """Convert options dict to list of command-line arguments for yt-dlp."""

    if isinstance(opts, list):
        return opts  # Already a list

    args = []
    if not isinstance(opts, dict):
        return args

    # Output directory
    temp_dir = config_manager.get("Paths", "temporary_downloads_directory", fallback="")
    output_dir = temp_dir or config_manager.get("Paths", "completed_downloads_directory", fallback="")
    if output_dir:
        try:
            os.makedirs(output_dir, exist_ok=True)
            # Test write permissions
            test_file = os.path.join(output_dir, ".test_write")
            try:
                with open(test_file, 'w') as f:
                    f.write("test")
            finally:
                if os.path.exists(test_file):
                    try:
                        os.remove(test_file)
                    except Exception:
                        pass
            
            # Removed length restrictions from title and uploader
            default_template = "%(title)s [%(uploader)s][%(release_date>%m-%d-%Y)s][%(id)s].%(ext)s"
            configured_template = config_manager.get("General", "output_template", fallback=default_template)
            output_template = os.path.join(output_dir, configured_template)
            # Normalize path separators for yt-dlp
            output_template = output_template.replace("\\", "/")
            
            if opts.get("use_gallery_dl", False):
                # gallery-dl uses -d for directory
                args.extend(["-d", output_dir])
            else:
                args.extend(["-o", output_template])
        except (PermissionError, OSError) as e:
            log.error(f"Output directory is not writable or accessible: {output_dir} - {e}")
            raise ValueError(f"Output directory is not writable or accessible: {output_dir}. Please check permissions or select a different directory.")

    if opts.get("use_gallery_dl", False):
        # gallery-dl specific arguments
        
        # Cookies
        cookies_browser = config_manager.get("General", "gallery_cookies_from_browser", fallback="None")
        if cookies_browser and cookies_browser != "None":
            args.extend(["--cookies-from-browser", cookies_browser])

        return args

    args.append("--newline")
    # Force utf-8 encoding for stdout/stderr to avoid character mapping issues
    args.extend(["--encoding", "utf-8"])
    # Continue on download errors
    args.append("--ignore-errors")

    # --- Audio/Video Format Selection ---
    audio_only = opts.get("audio_only")
    metadata_only = opts.get("metadata_only")

    if metadata_only:
        # Skip downloading the video/audio content
        args.append("--skip-download")
        # Ensure metadata is written
        args.append("--write-info-json")
        # We don't need format selection for metadata only, but yt-dlp might complain if none is set?
        # Actually, --skip-download usually ignores format selection, but let's be safe.
        
    elif audio_only:
        audio_codec = opts.get("acodec") or config_manager.get("General", "acodec", fallback="")
        format_string = "bestaudio/best"
        if audio_codec:
            # Prioritize the selected codec, but fall back to bestaudio if not available
            format_string = f"bestaudio[acodec~={audio_codec}]/bestaudio/best"

        args.extend(["-f", format_string])
        args.append("--extract-audio")
        audio_ext = opts.get("audio_ext") or config_manager.get("General", "audio_ext", fallback="mp3")
        args.extend(["--audio-format", audio_ext])

        audio_quality = opts.get("audio_quality") or config_manager.get("General", "audio_quality", fallback="best")
        if audio_quality and audio_quality != "best":
            quality_val = str(audio_quality).replace("k", "").replace("K", "")
            args.extend(["--audio-quality", quality_val])

        # Embed metadata from the source (e.g., artist, title) and embed the thumbnail as album art.
        # This is crucial for getting a complete audio file.
        args.append("--embed-metadata")
        args.append("--embed-thumbnail")

    else:
        # Build a format selector string for video + audio
        video_format_parts = []
        video_quality = opts.get("video_quality") or config_manager.get("General", "video_quality", fallback="best")
        if video_quality and video_quality != "best":
            try:
                height = int(video_quality.replace("p", "").replace("P", ""))
                video_format_parts.append(f"[height<={height}]")
            except ValueError:
                log.warning(f"Invalid video quality format: {video_quality}, ignoring.")

        video_codec = opts.get("vcodec") or config_manager.get("General", "vcodec", fallback="")
        if video_codec:
            # Use `~=` for fuzzy matching (e.g., avc1.xxxxxx matches avc1)
            video_format_parts.append(f"[vcodec~={video_codec}]")

        video_ext = opts.get("video_ext") or config_manager.get("General", "video_ext", fallback="")
        if video_ext:
            video_format_parts.append(f"[ext={video_ext}]")

        # Audio format parts (for the audio component of the video)
        audio_format_parts = []
        audio_codec = opts.get("video_acodec") or config_manager.get("General", "video_acodec", fallback="")
        if audio_codec:
            audio_format_parts.append(f"[acodec~={audio_codec}]")

        # Combine parts into final format string
        video_selector = "bestvideo" + "".join(video_format_parts)
        audio_selector = "bestaudio" + "".join(audio_format_parts)
        
        # Final format string: e.g., "bestvideo[height<=1080]+bestaudio/best"
        format_string = f"{video_selector}+{audio_selector}/best"
        args.extend(["-f", format_string])

        # Specify the final container format after merging
        merge_ext = video_ext or config_manager.get("General", "video_ext", fallback="")
        if merge_ext:
            args.extend(["--merge-output-format", merge_ext])

        # Embed metadata (title, artist, etc.) and thumbnail as album art for videos as well
        args.append("--embed-metadata")
        args.append("--embed-thumbnail")

    # --- Other Options ---
    playlist_mode = opts.get("playlist_mode", "Ask")
    if "ignore" in playlist_mode.lower() or "single" in playlist_mode.lower():
        args.append("--no-playlist")
    elif "all" in playlist_mode.lower() or "no prompt" in playlist_mode.lower():
        args.append("--yes-playlist")

    if config_manager.get("General", "sponsorblock", fallback="True") == "True":
        args.append("--sponsorblock-remove")
        args.append("sponsor,intro,outro,selfpromo,interaction,preview,music_offtopic")

    windows_cfg = config_manager.get("General", "windowsfilenames", fallback=None)
    restrict_cfg = config_manager.get("General", "restrict_filenames", fallback=None)
    if str(windows_cfg) == "True":
        args.append("--windows-filenames")
    elif str(restrict_cfg) == "True":
        args.append("--restrict-filenames")
    elif os.name == 'nt':
        args.append("--windows-filenames")

    # Replace pipe characters to prevent filename issues.
    # The standard pipe '|' is a regex metacharacter and must be escaped.
    # The full-width pipe '｜' is not, but we replace it for consistency.
    args.extend(["--replace-in-metadata", "title", r"\|", "-"])
    args.extend(["--replace-in-metadata", "title", "｜", "-"])

    # Replace invalid Windows characters to prevent yt-dlp from using full-width replacements
    args.extend(["--replace-in-metadata", "title", r"\?", ""])
    args.extend(["--replace-in-metadata", "title", r":", " -"])
    
    # Explicitly replace all forms of quotes with a simple apostrophe to avoid filename issues
    # Apply to title, album, artist, playlist_title, chapter, chapter_title
    for field in ["title", "album", "artist", "playlist_title", "chapter", "chapter_title"]:
        args.extend(["--replace-in-metadata", field, r'"', "'"])
        args.extend(["--replace-in-metadata", field, r"“", "'"])
        args.extend(["--replace-in-metadata", field, r"”", "'"])
        args.extend(["--replace-in-metadata", field, r"‘", "'"])
        args.extend(["--replace-in-metadata", field, r"’", "'"])
    
    # Force writing metadata to a JSON file so we can read it reliably
    # This is already added if metadata_only is True, but harmless to add again or ensure it's there
    if "--write-info-json" not in args:
        args.append("--write-info-json")

    # Enforce media date metadata precedence for embedded tags:
    # 1) release_date (YYYYMMDD), 2) release_year (YYYY), 3) upload_date (YYYYMMDD).
    # Use strict regex parses so malformed values do not overwrite valid ones.
    args.extend(["--parse-metadata", r"%(upload_date|)s:(?P<meta_date>\d{8})"])
    args.extend(["--parse-metadata", r"%(release_year|)s:(?P<meta_date>\d{4})"])
    args.extend(["--parse-metadata", r"%(release_date|)s:(?P<meta_date>\d{8})"])

    raw_rate_limit = opts.get("rate_limit", config_manager.get("General", "rate_limit", fallback=""))
    rate_limit = _normalize_rate_limit(raw_rate_limit)
    if rate_limit:
        args.extend(["--limit-rate", rate_limit])
        
    # --- Cookies ---
    cookies_browser = config_manager.get("General", "cookies_from_browser", fallback="None")
    if cookies_browser and cookies_browser != "None":
        args.extend(["--cookies-from-browser", cookies_browser])

    # --- JavaScript Runtime ---
    js_runtime_path = config_manager.get("General", "js_runtime_path", fallback="")
    # First, try user-configured path
    if js_runtime_path and os.path.exists(js_runtime_path):
        log.debug(f"Using user-configured JavaScript runtime at {js_runtime_path}")
    else:
        if js_runtime_path:  # Path was configured but not found
            log.warning(f"Configured JavaScript runtime not found at: {js_runtime_path}. Falling back to bundled.")
        # If not configured or not found, try to use the bundled deno
        js_runtime_path = get_binary_path("deno")

    if js_runtime_path and os.path.exists(js_runtime_path):
        # yt-dlp expects the runtime name (e.g., "deno") followed by its path
        runtime_name = os.path.basename(js_runtime_path).split('.')[0]  # "deno" from "deno.exe"
        args.extend(["--js-runtimes", f"{runtime_name}:{js_runtime_path}"])
        log.debug(f"Using JavaScript runtime: {runtime_name} at {js_runtime_path}")
    else:
        log.debug("No valid JavaScript runtime found (neither configured nor bundled).")

    return args
