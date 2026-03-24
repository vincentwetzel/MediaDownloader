# MediaDownloader C++ Specification

## 1. Overview
This document outlines the specifications for the C++ port of the MediaDownloader application. The goal is to create a drop-in replacement for the existing Python application, ensuring 100% feature parity and seamless transition for users.

## 2. Core Requirements

### 2.1. Single Instance Enforcement
- The application must ensure that only one instance of itself can run at any given time. Attempts to launch a second instance should result in the new instance exiting gracefully.

### 2.2. Configuration Compatibility
- **File Format**: `settings.ini` (INI format).
- **Location**: Application root directory or user data directory.
- **Parsing**: Must handle raw strings (no interpolation).
- **Canonicalization**: The app must rewrite `settings.ini` into a canonical layout on save so each persisted setting name appears only once, legacy sections like `[%General]` are removed, and deprecated aliases fall back to the canonical key names.
- **Keys**: Must support all keys from the Python version's `ConfigManager`, including:
    - `subtitles_embed`, `subtitles_write`, `subtitles_langs`, etc.
    - `restrict_filenames`, `exit_after`.
    - `convert_thumbnails`, `high_quality_thumbnail`.
    - `use_aria2c`.
    - `output_template`.
    - `cookies_from_browser`, `gallery_cookies_from_browser`.
    - `theme`.
    - `playlist_logic`, `max_threads`, `rate_limit`.
    - `override_archive`.
    - `embed_chapters`, `embed_metadata`, `embed_thumbnail`.
    - `video_quality`, `video_ext`, `vcodec`, `audio_quality`, `audio_ext`, `acodec`.
    - `lock_settings` for video and audio.

### 2.3. Archive Compatibility
- **Database**: `download_archive.db` (SQLite).
- **Schema**: Must match the Python version's schema exactly.
- **Logic**: URL normalization and duplicate detection logic must be identical to the Python version.

### 2.4. User Interface (Qt Widgets)
- **Main Window**: Tabbed interface with a footer containing links to GitHub and Discord.
- **Start Tab**:
    - URL Input.
    - Clipboard auto-paste: when the URL field is focused/clicked, the app checks the clipboard against the extractor-domain list stored next to `MediaDownloader.exe` and auto-pastes a matching URL.
    - If `auto_paste_on_focus` is enabled, focusing or hovering the app window will switch to Start Download and auto-paste when a valid clipboard URL is detected.
    - Download Type dropdown, including "View Formats".
    - Video Settings group with quality, codec, extension, and audio codec for video downloads. Includes a "Lock Video Settings" checkbox.
    - Audio Settings group with quality, codec, and extension for audio downloads. Includes a "Lock Audio Settings" checkbox.
    - Operational Controls including Playlist logic, Max Concurrent downloads, Rate Limit, "Override duplicate download check", and "Exit after all downloads complete".
- **Active Downloads Tab**:
    - Displays a list of queued, actively downloading, and completed items.
    - Each download GUI element must play/display a thumbnail preview for audio/video downloads on the left side of the widget.
- **Advanced Settings Tab**:
    - **Organization**: Settings are grouped into logical sections:
        - **Configuration**: Output folder, Temporary folder, Theme.
        - **Authentication Access**: Cookies from browser (Video/Audio), Cookies from browser (Galleries). The cookie access check is handled directly within `AdvancedSettingsTab` using `QProcess`, with a 30-second timeout and improved logging. The check uses a specific YouTube Shorts URL for more reliable validation.
        - **Output Template**: Filename Pattern (with "Insert token...", "Save", and "Reset" buttons). The "Save" button validates the pattern using `yt-dlp`.
        - **Download Options**: External Downloader (aria2c), Enable SponsorBlock, Restrict filenames, Embed video chapters, Auto-paste URL when app is focused.
        - **Metadata / Thumbnails**: Embed metadata, Embed thumbnail, Use high-quality thumbnail converter, Convert thumbnails to.
        - **Subtitles**: Subtitle language (using full words in a combo box), Embed subtitles in video, Write subtitles (separate file), Include automatically-generated subtitles, Subtitle file format (greyed out if "Embed subtitles in video" is selected).
        - **Updates**: `yt-dlp` version (display only), Update `yt-dlp` (always to nightly), `gallery-dl` version (display only), Update `gallery-dl`.
        - **Restore defaults** button.
    - **Saving Behavior**: Most settings auto-save on change. The "Output Template" requires a dedicated "Save" button.
- **System Integration**: A system tray icon for quick show/hide and quit actions. Clicking the window close button (`X`) must exit the application (it must not keep running in the background).
- **Theming**: Support for Light, Dark, and System themes.

### 2.5. Download Engine (yt-dlp & gallery-dl)
- **Execution**: `QProcess` to run `yt-dlp.exe` or `gallery-dl.exe`.
- **Bundled Binary Resolution**: `yt-dlp.exe` launch must support both deployment layouts (`<appDir>/yt-dlp.exe` and `<appDir>/bin/yt-dlp.exe`).
- **Launch Error Handling**: If process start fails (`QProcess::FailedToStart` or related launch errors), the download must transition to a terminal error state with a clear message (no indefinite "Downloading..." state).
- **Argument Construction**: Must dynamically build arguments based on all user settings, including:
    - Subtitle flags (`--write-subs`, `--embed-subs`, `--sub-langs`).
    - Filename restriction (`--restrict-filenames`).
    - External downloader (`--external-downloader aria2c`).
    - Thumbnail conversion (`--convert-thumbnails`).
    - SponsorBlock (`--sponsorblock`).
    - Cookies from browser (`--cookies-from-browser`).
    - Output template (`-o`).
    - Max concurrent downloads (`--concurrent-fragments`).
    - Rate limit (`--limit-rate`).
    - Override archive (`--no-download-archive`).
    - Embed chapters (`--embed-chapters`).
- **Output Parsing**: Must parse `yt-dlp` stdout for progress, final filename, and metadata JSON.
- **Encoding Robustness**: Worker process environment must force UTF-8 text output (`PYTHONUTF8=1`, `PYTHONIOENCODING=utf-8`) so Unicode filenames are preserved in stdout/stderr parsing.

### 2.6. Post-Processing
- **File Lifecycle**:
    - All downloads must go to a temporary directory first.
    - A file stability check must be performed before moving.
    - Files are moved to a final destination directory upon success.
    - Final file movement must be Unicode-safe and shell-independent (use Qt file APIs with copy/remove fallback for cross-volume moves).
- **Audio Playlist Tagging**: For audio downloads from a playlist, `ffmpeg` must be used as a post-processing step to embed the `track` number metadata into the completed file.
- **Filename Prefixing**: Audio files from playlists must have their filenames prefixed with a zero-padded track number (e.g., `01 - Title.mp3`).

### 2.7. Updaters & Deployment
- **Application Updater**: Checks GitHub for new application releases and provides a download/install mechanism.
- **yt-dlp Updater**: Provides a mechanism for the user to update the `yt-dlp.exe` binary from within the app. It will always update to the latest nightly build. The current `yt-dlp` version must be displayed in the Advanced Settings.
- **Executable Name**: Must be `MediaDownloader.exe`.
- **Binaries**: Must bundle `yt-dlp.exe`, `ffmpeg.exe`, `ffprobe.exe`, `gallery-dl.exe`, `aria2c.exe`, and `deno.exe`.
- **Qt Image Plugins**: Windows builds must deploy the Qt `imageformats` plugins required to display active-download thumbnails and converted artwork, including JPEG, PNG, WebP, and ICO support.

### 2.8. Logging
- A structured, rotating file logger (`MediaDownloader.log`) must be implemented to capture application output for debugging.

## 3. Technical Stack
- **Language**: C++20
- **Framework**: Qt 6 (Widgets)
- **Build System**: CMake
- **Database**: SQLite (via Qt SQL module)
- **Process Management**: `QProcess`
