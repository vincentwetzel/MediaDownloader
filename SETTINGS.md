# Settings Reference Guide

This document provides a complete reference for all configuration settings used by LzyDownloader (C++ Port). Settings are stored in `settings.ini` using Qt's native `QSettings` INI format.

> **Portability Note:** The C++ port uses a pure Qt-native INI format. Backwards compatibility with the Python application's `configparser` quirks is no longer required, and invalid/legacy keys are pruned or reset to defaults as needed.

---

## Table of Contents

- [General](#general)
- [Paths](#paths)
- [Video](#video)
- [Audio](#audio)
- [Metadata](#metadata)
- [Subtitles](#subtitles)
- [Binaries](#binaries)
- [DownloadOptions](#downloadoptions)
- [SortingRules](#sortingrules)
- [MainWindow / UI / Geometry](#mainwindow--ui--geometry)

---

## General

Application-wide settings that control theme, cookie handling, clipboard behavior, and other global options.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `output_template` | String | `%(title)s [%(uploader)s][%(upload_date>%Y-%m-%d)s][%(id)s].%(ext)s` | Default filename template for yt-dlp downloads. Used as fallback when type-specific templates are not set. |
| `gallery_output_template` | String | `{category}/{id}_{filename}.{extension}` | Filename template for gallery-dl downloads. Uses gallery-dl's own template syntax. |
| `theme` | String | `System` | Application visual theme. Options: `System`, `Light`, `Dark`. |
| `cookies_from_browser` | String | `None` | Browser to extract cookies from for video/audio downloads. Options: installed browser names (e.g., `Firefox`, `Chrome`) or `None`. |
| `gallery_cookies_from_browser` | String | `None` | Browser to extract cookies from for gallery downloads. Options: same as above or `None`. |
| `sponsorblock` | Boolean | `false` | Enable SponsorBlock integration to skip sponsored segments in downloaded videos. Uses forced keyframes to preserve A/V sync. |
| `auto_paste_mode` | Integer | `0` | Clipboard auto-paste behavior. See [Auto-Paste Modes](#auto-paste-modes) below. |
| `single_line_preview` | Boolean | `false` | Display the command preview in single-line mode on the Start tab. |
| `restrict_filenames` | Boolean | `false` | Restrict downloaded filenames to ASCII characters only for maximum compatibility. |
| `max_threads` | String | `4` | Maximum number of concurrent downloads to start automatically. Users may raise this during a session, but startup clamps persisted values back to `4` to avoid aggressive resume storms. |
| `playlist_logic` | String | `Ask` | Default playlist handling mode. Options: `Ask`, `Download All (no prompt)`, `Download Single (ignore playlist)`. |
| `rate_limit` | String | `Unlimited` | Global yt-dlp rate limit preset shown on the Start tab. |
| `override_archive` | Boolean | `false` | Allow downloads that would otherwise be blocked by archive/duplicate detection. |
| `exit_after` | Boolean | `false` | Exit the app automatically after the queue fully finishes. The delayed shutdown re-checks queued and active counts before quitting. |
| `language` | String | `🇺🇸 English` | UI language selector value stored by the main window. |
| `show_debug_console` | Boolean | `true` (Debug) / `false` (Release) | Show or hide the command prompt / debug console window while the application is running. |
| `warn_stable_yt_dlp` | Boolean | `true` | Controls whether the runtime popup warns when the detected `yt-dlp` build looks like a stable release instead of a nightly build. This preference is currently changed from the popup itself, not a dedicated settings page. |

### Auto-Paste Modes

| Index | Mode | Behavior |
|-------|------|----------|
| `0` | Disabled | No automatic pasting. |
| `1` | Auto-paste on app focus or hover | Pastes clipboard content when the application gains focus or the URL field is hovered. |
| `2` | Auto-paste on new URL in clipboard | Pastes when a new URL is detected in the clipboard. |
| `3` | Auto-paste & enqueue on app focus | Pastes and immediately adds to the download queue when the app gains focus. |
| `4` | Auto-paste & enqueue on new URL in clipboard | Pastes and enqueues when a new URL is detected in the clipboard. |

Internally, clipboard-triggered auto-paste uses a short debounce window of roughly 500 ms plus queue-level duplicate detection, so rapid clipboard notifications do not enqueue the same URL multiple times while still allowing quick successive copies.

---

## Paths

File system locations for download directories.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `completed_downloads_directory` | String | *(user prompt on first launch)* | The directory where finished downloads are moved to after verification. |
| `temporary_downloads_directory` | String | *(derived from completed_downloads_directory)* | The directory used for active/in-progress downloads. Automatically set when `completed_downloads_directory` is updated. |

> **Note:** The `temporary_downloads_directory` is automatically derived from the `completed_downloads_directory` if not explicitly set. Downloads follow a lifecycle: download to temp dir → verify file stability → move to completed directory.

---

## Video

Default settings for video downloads. These options are bypassed when "Select at Runtime" is chosen for quality.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `video_quality` | String | `best` | Default video quality. Options: `Select at Runtime`, `best`, `2160p`, `1440p`, `1080p`, `720p`, `480p`, `360p`, `240p`, `144p`, `worst`. |
| `video_codec` | String | `H.264 (AVC)` | Default video codec. Options: `Default`, `H.264 (AVC)`, `H.265 (HEVC)`, `VP9`, `AV1`, `ProRes (Archive)`, `Theora`. |
| `video_extension` | String | `mp4` | Default video file extension. Options change dynamically based on selected codec. |
| `video_audio_codec` | String | `AAC` | Default audio codec embedded in video files. Options: `Default`, `AAC`, `Opus`, `Vorbis`, `MP3`, `FLAC`, `PCM`. |
| `video_multistreams` | String | `Default Stream` | Controls multi-stream selection behavior for video downloads. |

### Codec-Extension Compatibility

| Video Codec | Available Extensions | Available Audio Codecs |
|-------------|---------------------|------------------------|
| H.264 (AVC) | `mp4`, `mkv` | `Default`, `AAC`, `MP3`, `FLAC`, `PCM` |
| H.265 (HEVC) | `mp4`, `mkv` | `Default`, `AAC`, `MP3`, `FLAC`, `PCM` |
| VP9 | `webm`, `mkv` | `Default`, `Opus`, `Vorbis`, `AAC` |
| AV1 | `webm`, `mkv` | `Default`, `Opus`, `Vorbis`, `AAC` |
| ProRes (Archive) | `mov` | `Default`, `PCM`, `AAC` |
| Theora | `ogv` | `Default`, `Vorbis` |
| Default | `mp4`, `mkv`, `webm` | `Default`, `AAC`, `Opus`, `Vorbis`, `MP3`, `FLAC`, `PCM` |

---

## Audio

Default settings for audio-only downloads. These options are bypassed when "Select at Runtime" is chosen for quality.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `audio_quality` | String | `best` | Default audio quality. Options: `Select at Runtime`, `best`, `320k`, `256k`, `192k`, `128k`, `96k`, `64k`, `32k`, `worst`. |
| `audio_codec` | String | `Opus` | Default audio codec. Options: `Default`, `Opus`, `AAC`, `Vorbis`, `MP3`, `FLAC`, `WAV`, `ALAC`, `AC3`, `EAC3`, `DTS`, `PCM`. |
| `audio_extension` | String | `opus` | Default audio file extension. Options change dynamically based on selected codec. |
| `audio_multistreams` | String | `Default Stream` | Controls multi-stream selection behavior for audio downloads. |

### Codec-Extension Compatibility (Audio)

| Audio Codec | Available Extensions |
|-------------|---------------------|
| Opus | `opus` |
| AAC | `m4a`, `aac` |
| Vorbis | `ogg` |
| MP3 | `mp3` |
| FLAC | `flac` |
| WAV / PCM | `wav` |
| ALAC | `m4a`, `alac` |
| AC3 / EAC3 / DTS | `ac3`, `eac3`, `dts` |
| Default | `mp3`, `m4a`, `opus`, `wav`, `flac` |

---

## Livestream

Settings specific to downloading live broadcasts.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `live_from_start` | Boolean | `false` | Record the livestream from the beginning instead of the current live edge. |
| `wait_for_video` | Boolean | `true` | Wait for an upcoming livestream to start instead of failing immediately. |
| `wait_for_video_min` | Integer | `60` | Minimum seconds to wait between retries. |
| `wait_for_video_max` | Integer | `300` | Maximum seconds to wait between retries. |
| `download_as` | String | `MPEG-TS` | Format to download the livestream. Options: `MPEG-TS`, `MKV`. |
| `use_part` | Boolean | `true` | Use `.part` files during download. |
| `quality` | String | `best` | Quality of the livestream. Options: `best`, `1080p`, `720p`, etc. |
| `convert_to` | String | `None` | Convert the finished stream to another format via FFmpeg. Options: `None`, `mp4`, `mkv`, `flv`, `webm`, etc. |

---

## Metadata

Settings for embedding metadata, thumbnails, and chapter information into downloaded files.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `use_aria2c` | Boolean | `true` | Use aria2c as an external downloader for segmented, concurrent downloads. |
| `embed_chapters` | Boolean | `true` | Embed chapter markers into video files when available. |
| `embed_metadata` | Boolean | `true` | Embed metadata (title, artist, description, etc.) into downloaded files. |
| `embed_thumbnail` | Boolean | `true` | Embed thumbnail images into downloaded files as cover art. |
| `high_quality_thumbnail` | Boolean | `true` | Use a higher-quality thumbnail source when available. |
| `convert_thumbnail_to` | String | `jpg` | Convert embedded thumbnails to this format. Options: `None`, `jpg`, `png`. |
| `crop_artwork_to_square` | Boolean | `true` | Crop audio thumbnails to square aspect ratio. |
| `generate_folder_jpg` | Boolean | `false` | Generate a `folder.jpg` file for audio playlists. |

---

## Subtitles

Configuration for subtitle downloading and embedding.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `languages` | String | `en` | Comma-separated list of subtitle language codes to download (e.g., `en,es,fr`). Supports special values: `runtime`, `all`, `auto`, `live_chat`. |
| `embed_subtitles` | Boolean | `true` | Embed subtitles directly into the video file when available. |
| `write_subtitles` | Boolean | `false` | Write subtitles to separate files alongside the video. |
| `write_auto_subtitles` | Boolean | `true` | Include automatically-generated subtitles when downloading. |
| `format` | String | `srt` | Subtitle file format. Options: `srt`, `vtt`, `ass`. Disabled when `embed_subtitles` is enabled. |

### Available Subtitle Languages

The subtitle language picker includes the following options:

**Special Values:**
- `runtime` — Select at Runtime
- `all` — All available languages
- `auto` — Auto-generated subtitles only
- `live_chat` — Live chat transcripts

**Language Codes:** `en` (English), `es` (Spanish), `fr` (French), `de` (German), `it` (Italian), `pt` (Portuguese), `ru` (Russian), `ja` (Japanese), `ko` (Korean), `zh-Hans` (Chinese Simplified), `zh-Hant` (Chinese Traditional), `ar` (Arabic), `hi` (Hindi), `bn` (Bengali), `pa` (Punjabi), `tr` (Turkish), `vi` (Vietnamese), `id` (Indonesian), `nl` (Dutch), `pl` (Polish), `sv` (Swedish), `fi` (Finnish), `no` (Norwegian), `da` (Danish), `el` (Greek), `cs` (Czech), `hu` (Hungarian), `ro` (Romanian), `uk` (Ukrainian), `th` (Thai).

---

## Binaries

Manual path overrides for external executables. If not set, the application auto-detects binaries from the system PATH or user-local install locations.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `yt-dlp_path` | String | *(auto-detected)* | Path to the `yt-dlp` executable. **Required.** |
| `ffmpeg_path` | String | *(auto-detected)* | Path to the `ffmpeg` executable. **Required.** |
| `ffprobe_path` | String | *(auto-detected)* | Path to the `ffprobe` executable. **Required.** |
| `deno_path` | String | *(auto-detected)* | Path to the `deno` executable. **Required** (for JS runtime support). |
| `gallery-dl_path` | String | *(auto-detected)* | Path to the `gallery-dl` executable. *Optional.* |
| `aria2c_path` | String | *(auto-detected)* | Path to the `aria2c` executable. *Optional.* |

> **Binary Resolution Order:** The application searches in this order: (1) User-configured path, (2) System PATH, (3) User-local install locations (e.g., `~/.deno/bin`, scoop shims, WindowsApps, Chocolatey).

---

## DownloadOptions

Additional download behavior and UI preferences.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `split_chapters` | Boolean | `false` | Split video chapters into separate files during download. |
| `download_sections_enabled` | Boolean | `false` | If enabled, a dialog will appear before downloading to let you specify time ranges or chapters to download. |
| `auto_clear_completed` | Boolean | `false` | Automatically clear completed downloads from the Active Downloads tab. |
| `geo_verification_proxy` | String | *(empty)* | Proxy URL for geo-restricted content (e.g., `http://proxy.server:port`). |

---

## SortingRules

User-defined rules for organizing downloaded files. Stored as numbered sections.

### Structure

Sorting rules are stored under the `[SortingRules]` section with a `count` key and indexed entries:

```ini
[SortingRules]
count=2
1={"condition":"type","target":"Movies/%(title)s","type":"video","value":"movie"}
2={"condition":"uploader","target":"Channels/%(uploader)s/%(title)s","value":"some_channel"}
```

### Rule Fields

| Field | Type | Description |
|-------|------|-------------|
| `type` | String | Download type filter: `video`, `audio`, `gallery`, or `any`. |
| `condition` | String | Matching condition. See [Conditions](#conditions) below. |
| `value` | String | The value(s) to match against (depends on condition). |
| `target` | String | The output path template if the rule matches. |

### Conditions

| Condition | Description | Value Format |
|-----------|-------------|--------------|
| `type` | Matches the download type. | `video`, `audio`, `gallery` |
| `uploader` | Matches the content uploader/channel. | Uploader name |
| `url_contains` | URL contains a specific string. | Substring to match |
| `is_one_of` | Value is one of a list. | Comma-separated list (sorted alphabetically on save) |
| `greater_than` | Numeric comparison (greater than). | Numeric threshold |
| `less_than` | Numeric comparison (less than). | Numeric threshold |

> **Priority:** Rules are evaluated in priority order (1 = highest). The first matching rule determines the output directory via the `SortingManager`.

---

## MainWindow / UI / Geometry

Internal settings for window state and UI layout. These are managed automatically by Qt and should not be edited manually.

| Key Group | Description |
|-----------|-------------|
| `MainWindow` | Main window geometry, position, and state (maximized, etc.). |
| `UI` | UI-related preferences such as splitter states and tab indices. |
| `Geometry` | Additional geometric configuration for dialogs and widgets. |

---

## Settings File Location

The `settings.ini` file is stored in the system's standard user configuration directory:
- **Windows:** `%LOCALAPPDATA%\LzyDownloader\settings.ini`
- **Linux:** `~/.config/LzyDownloader/settings.ini`
- **macOS:** `~/Library/Application Support/LzyDownloader/settings.ini`

This ensures user settings are preserved across application updates and installations via the NSIS installer.

## Queue Backup Location

Active, paused, and stopped downloads are automatically serialized to a JSON file so they can be resumed across application restarts:
- **Windows:** `%LOCALAPPDATA%\LzyDownloader\downloads_backup.json`
- **Linux:** `~/.config/LzyDownloader/downloads_backup.json`
- **macOS:** `~/Library/Application Support/LzyDownloader/downloads_backup.json`

Stopped and failed entries also retain the latest known temporary file paths needed for resume and cleanup workflows. This allows the Active Downloads tab's `Clear Temp Files` action to remove tracked partial media, sidecar metadata, thumbnails, and downloader state files even after an app restart.

## Log File Location

Application logs are stored in the same user configuration directory:
- **Windows:** `%LOCALAPPDATA%\LzyDownloader\LzyDownloader_YYYY-MM-dd_HH-mm-ss.log`
- **Linux:** `~/.config/LzyDownloader/LzyDownloader_YYYY-MM-dd_HH-mm-ss.log`
- **macOS:** `~/Library/Application Support/LzyDownloader/LzyDownloader_YYYY-MM-dd_HH-mm-ss.log`

**Log Retention:** Each app launch creates a fresh timestamped log file, and startup cleanup keeps only the 5 most recent logs. Legacy size-rotated `LzyDownloader.log.*` files are also deleted if they are still present from older builds.

### Canonical Format

The C++ port rewrites `settings.ini` on save to ensure a canonical layout:
- Removes legacy sections like `[%General]`
- Removes deprecated duplicate keys
- Preserves all valid settings including `SortingRules`
- Maintains compatibility with Python's `configparser`

### Reset to Defaults

When resetting to defaults via Advanced Settings, the following are **preserved**:
- **Path settings:** `completed_downloads_directory`, `temporary_downloads_directory`
- **Theme:** `theme`
- **Output templates:** `output_template`, `output_template_video`, `output_template_audio`, `gallery_output_template`
- **Binary paths:** All `Binaries/*` settings
- **Sorting rules:** Entire `[SortingRules]` group
- **Window state:** `MainWindow`, `UI`, `Geometry` groups

All other settings are reset to their default values.

---

## Quick Reference: Settings by UI Location

| UI Location | Setting Section | Key(s) |
|-------------|----------------|--------|
| **Advanced Settings → Configuration** | `Paths` | `completed_downloads_directory`, `temporary_downloads_directory` |
| **Advanced Settings → Configuration** | `General` | `theme` |
| **Advanced Settings → Authentication Access** | `General` | `cookies_from_browser`, `gallery_cookies_from_browser` |
| **Advanced Settings → Output Template** | `General` | `output_template`, `output_template_video`, `output_template_audio`, `gallery_output_template` |
| **Advanced Settings → Download Options** | `Metadata` | `use_aria2c` |
| **Advanced Settings → Download Options** | `General` | `sponsorblock`, `auto_paste_mode`, `single_line_preview`, `restrict_filenames`, `embed_chapters` |
| **Advanced Settings → Download Options** | `DownloadOptions` | `split_chapters`, `download_sections_enabled`, `auto_clear_completed`, `geo_verification_proxy` |
| **Advanced Settings → Configuration** | `General` | `show_debug_console` |
| **Advanced Settings → Metadata** | `Metadata` | `embed_metadata`, `embed_thumbnail`, `high_quality_thumbnail`, `convert_thumbnail_to`, `crop_artwork_to_square`, `generate_folder_jpg` |
| **Advanced Settings → Subtitles** | `Subtitles` | `languages`, `embed_subtitles`, `write_subtitles`, `write_auto_subtitles`, `format` |
| **Advanced Settings → External Binaries** | *(N/A - runtime only)* | yt-dlp/gallery-dl version display and update buttons |
| **Advanced Settings → External Binaries** | `Binaries` | `yt-dlp_path`, `ffmpeg_path`, `ffprobe_path`, `gallery-dl_path`, `aria2c_path`, `deno_path` |
| **Start Tab** | `Video` | `video_quality`, `video_codec`, `video_extension`, `video_audio_codec`, `video_multistreams` |
| **Start Tab** | `Audio` | `audio_quality`, `audio_codec`, `audio_extension`, `audio_multistreams` |
| **Sorting Tab** | `SortingRules` | `count`, `1`, `2`, ... |

---

## Troubleshooting

### Invalid Settings

If a setting loaded from `settings.ini` does not match the current application's expected format, it is automatically discarded and replaced with the default value. The application does not attempt to interpret legacy formats.

### Cookie Database Locked

If you see a "database is locked" error when selecting a browser for cookies, close the browser completely and try again. The browser must not be running to access its cookie database.

### Missing Binaries

If a required binary (`yt-dlp`, `ffmpeg`, `ffprobe`, `deno`) is not found, use the **Install...** button in Advanced Settings → Binaries to launch package-manager or manual-download options.

---

*Last updated: April 3, 2026 — LzyDownloader C++ Port*
