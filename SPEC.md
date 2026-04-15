# LzyDownloader C++ Specification

## 1. Overview
This document outlines the specifications for the C++ port of the LzyDownloader application. The goal is to create a drop-in replacement for the existing Python application, ensuring 100% feature parity and seamless transition for users.

### 1.1 Documentation Requirement
- **Mandatory**: When any agent or developer makes changes to how the app works (e.g., progress parsing, download pipeline, UI behavior, configuration, external binary handling), they MUST update the relevant MD documentation files (`AGENTS.md`, `SPEC.md`, `ARCHITECTURE.md`, `TODO.md`, `CHANGELOG.md`) to reflect the new behavior. Documentation MUST stay in sync with the code.

### 1.2 File Size Constraints
- **Context Preservation**: To ensure optimal performance with AI agents and preserve context usage, no single file (source code, headers, or documentation) should exceed **500 lines** in length. When a file approaches this limit, it must be refactored or split into smaller, focused modules.

## 2. Core Requirements

### 2.1. Single Instance Enforcement
- The application must ensure that only one instance of itself can run at any given time. Attempts to launch a second instance should result in the new instance exiting gracefully.

### 2.2. Configuration Compatibility
- **File Format**: `settings.ini` (INI format).
- **Location**: Application root directory or user data directory.
- **Parsing**: Must handle raw strings (no interpolation).
- **Legacy Support**: Backwards compatibility with the Python application's settings file is NO LONGER required. The application uses standard Qt `QSettings` behavior and automatically prunes unrecognised or legacy keys on startup to maintain a clean configuration file.
    - `restrict_filenames`, `exit_after`.

**Required settings keys include:**
    - `output_template`.
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
    - `livestream/live_from_start`, `livestream/wait_for_video`, `livestream/download_as`, `livestream/use_part`, `livestream/quality`, `livestream/convert_to`.

### 2.3. Archive Compatibility
- **Database**: `download_archive.db` (SQLite).
- **Schema**: Must match the Python version's schema exactly.
- **Logic**: URL normalization and duplicate detection logic must be identical to the Python version.

### 2.4. User Interface (Qt Widgets)
- **Main Window**: Tabbed interface with a footer containing links to GitHub and Discord.
- **Start Tab**:
    - URL Input.
    - Clipboard auto-paste: when the URL field is focused/clicked, the app checks the clipboard against the extractor-domain list stored next to `LzyDownloader.exe` and auto-pastes a matching URL.
    - If `auto_paste_on_focus` is enabled, focusing or hovering the app window will switch to Start Download and auto-paste when a valid clipboard URL is detected. This logic is now handled by `src/ui/StartTabUrlHandler.h/.cpp`.
    - Download Type dropdown, including "View Formats".
    - No per-download runtime quality/codec override dropdowns may appear on the Start tab; runtime format selection must be driven by Advanced Settings and download-time dialogs.
    - Video Settings group with quality, codec, extension, and audio codec defaults. Choosing `Quality = Select at Runtime` must hide the other video-format defaults on that page and defer the whole format decision to the runtime picker. Includes a "Lock Video Settings" checkbox.
    - Audio Settings group with quality, codec, and extension defaults. Choosing `Quality = Select at Runtime` must hide the other audio-format defaults on that page and defer the whole format decision to the runtime picker. Includes a "Lock Audio Settings" checkbox.
    - Operational Controls including Playlist logic, Max Concurrent downloads, a global Rate Limit (app-wide, not per-download), and "Override duplicate download check". "Exit after all downloads complete" is controlled from the main window footer.
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
        - **Livestream Settings**: Record from beginning, Wait for video (with min/max intervals). The app dynamically scales these wait intervals for streams hours away vs seconds away. Download As (MPEG-TS or MKV), Use .part files, Quality, Convert To.
        - **Subtitles**: Subtitle language (using full words in a combo box), Embed subtitles in video, Write subtitles (separate file), Include automatically-generated subtitles, Subtitle file format (greyed out if "Embed subtitles in video" is selected).
        - **External Binaries**: Per-binary status rows for `yt-dlp`, `ffmpeg`, `ffprobe`, `gallery-dl`, `aria2c`, and `deno`, with auto-detection status, manual `Browse` overrides, and `Install` actions that offer detected package-manager commands plus manual-download links.
        - **Updates**: `yt-dlp` version (display only), Update `yt-dlp` (always to nightly), `gallery-dl` version (display only), Update `gallery-dl`.
        - **Restore defaults** button.
    - **Navigation Styling**: The left column uses a palette-aware `QListWidget` whose stylesheet is rebuilt on palette changes so the category list stays compact and theme-consistent without reverting to a plain scrollbar-heavy layout.
    - **Saving Behavior**: Most settings auto-save on change. The "Output Template" requires a dedicated "Save" button.
- **System Integration**: A system tray icon for quick show/hide and quit actions. Clicking the window close button (`X`) must exit the application (it must not keep running in the background).
- **Theming**: Support for Light, Dark, and System themes.

### 2.5. Download Engine (yt-dlp & gallery-dl)
- **Execution**: `QProcess` to run `yt-dlp.exe` or `gallery-dl.exe`.
- **Launch Error Handling**: If process start fails (`QProcess::FailedToStart` or related launch errors), the download must transition to a terminal error state with a clear message (no indefinite "Downloading..." state).
- **Binary Discovery**: On launch, the application must search for required external binaries in system `PATH` and other common locations. The UI must adapt based on which binaries are found.
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
- **Output Parsing**: Must parse `yt-dlp` stdout/stderr for progress, final filename, and metadata JSON.
- **Progress Compatibility**: The worker MUST understand and emit progress from **both** native `yt-dlp` progress lines **and** aria2c external downloader output, including:
  - Native yt-dlp format: `[download] XX.X% of YY.YMiB at ZZ.ZMiB/s ETA 0:00`
  - aria2c format: `[#XXXXX AA.AMiB/BB.BMiB(CC.C%)(...)ETA:0:00]`
  - Unknown or approximate totals (e.g., `~XX.XMiB`, `Unknown total size`)
  - UTF-8 filenames and metadata
  - **The progress bar MUST update correctly regardless of which downloader (native or aria2c) is active**
- **Progress Bar Color Coding**: The UI progress bar MUST use color-coding to provide clear visual feedback on download state:
  - **Colorless/Default** (no custom stylesheet): When download is queued, initializing, or in indeterminate state (progress < 0)
  - **Light Blue** (`#3b82f6`): While actively downloading (0% < progress < 100%)
  - **Teal** (`#008080`): During post-processing phase (progress at 100% with status containing "Processing", "Merging", or "Post")
  - **Green** (`#22c55e`): When download is fully completed (progress at 100% and post-processing finished)
- **Detailed Progress Information Display**: The application MUST provide comprehensive, real-time progress information to users, comparable to command-line yt-dlp output:
  - **Status Label**: Must display the current download stage with descriptive messages:
    - "Extracting media information..." (during metadata extraction)
    - "Downloading N segment(s)..." (during aria2c multi-segment downloads)
    - "Merging segments with ffmpeg..." (during post-processing)
    - "Verifying download completeness..." (during file stability checks)
    - "Applying sorting rules..." (during directory sorting)
    - "Moving to final destination..." / "Copying file to destination..." (during file movement)
    - "Embedding metadata..." (during metadata embedding for audio playlists)
    - "Next check in MM:SS" (while waiting for a scheduled livestream)
  - **Progress Details**: Below the progress bar, display formatted details including:
    - File sizes: "Downloaded / Total" (e.g., "15.3 MiB / 45.7 MiB")
    - Download speed (e.g., "Speed: 2.4 MiB/s")
    - Estimated time remaining (e.g., "ETA: 0:12")
  - All information MUST be separated by bullet points (•) for readability
  - Workers MUST emit detailed progress data including speed, ETA, downloaded_size, and total_size
- **Immediate Queue UI Feedback**: Downloads MUST appear in the Active Downloads tab immediately upon queuing:
  - **Gallery downloads**: Appear instantly with "Queued" status
  - **Video/Audio downloads**: Appear instantly with "Checking for playlist..." status while playlist expansion runs asynchronously
    - **Queue state persistence**: `DownloadQueueState` class handles saving/loading queue state, deferring calls via `Qt::QueuedConnection` to prevent GUI thread blocking.
  - **Single videos**: Status updates from "Checking for playlist..." to "Queued" once expansion completes. `DownloadQueueManager` handles updating the placeholder item.
  - **Playlists**: Placeholder item is removed and replaced with individual items for each track
  - Queue state persistence (handled by `DownloadQueueManager`) MUST be deferred via `Qt::QueuedConnection` to prevent GUI thread blocking
- **Runtime Format Selection**: When Advanced Settings `Quality` is set to `Select at Runtime` for video or audio downloads, the app must asynchronously fetch format metadata with `yt-dlp` and present a structured selection dialog. Selecting multiple formats must enqueue one download per selected format.
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
- **Dependency Management**:
  - Provides a UI for users to see the status of external binaries (`yt-dlp`, `ffmpeg`, etc.), manually locate them, or trigger an installation process.
  - The installation process will guide the user through automated (package manager) or manual (web download) methods.
- **Executable Name**: Must be `LzyDownloader.exe`.
- **Binaries**: The application does not bundle external binaries. It requires the user to have `ffmpeg`, `ffprobe`, `deno`, and `yt-dlp` installed. `gallery-dl` and `aria2c` are optional.
- **Qt Image Plugins**: Windows builds must deploy the Qt `imageformats` plugins required to display active-download thumbnails and converted artwork, including JPEG, PNG, WebP, and ICO support.

### 2.8. Logging
- A structured file logger (`LzyDownloader_YYYY-MM-dd_HH-mm-ss.log`) must be implemented to capture application output for debugging.
- **One log file per run**: Each application launch creates a new log file with a timestamp in the filename.
- **Log retention**: The application automatically keeps only the 10 most recent log files, deleting older ones on startup.
- Log files must be stored in the user's AppData configuration directory (`%LOCALAPPDATA%\LzyDownloader\` on Windows).

## 3. Technical Stack
- **Language**: C++20
- **Framework**: Qt 6 (Widgets)
- **Build System**: CMake
- **Qt SDK Discovery**: CMake must honor explicit `Qt6_DIR`/`CMAKE_PREFIX_PATH` configuration and also auto-check common Windows Qt install prefixes (for example `C:\Qt\6.*\msvc2022_64`) so IDE-driven configure steps can find Qt without manual edits on typical developer machines.
- **Database**: SQLite (via Qt SQL module)
- **Process Management**: `QProcess`
