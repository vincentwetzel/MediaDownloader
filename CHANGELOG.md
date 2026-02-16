# Changelog

All notable changes to MediaDownloader will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - YYYY-MM-DD

### Added
- **Version in Title Bar**: The application window title now includes the version number (e.g., "Media Downloader v0.0.7").
- **Advanced Filename Template Insertables**: Added an insertables dropdown next to `Filename Pattern` in Advanced Settings.
  - Users can click to insert common yt-dlp output tokens like `%(title)s`, `%(uploader)s`, `%(upload_date>%m)s`, `%(id)s`, and `%(ext)s`.
  - This mirrors the token-insertion workflow used by sorting subfolder patterns for faster, less error-prone template building.
- **Archive redownload override checkbox**: Added `Override duplicate download check` to the Start tab so users can bypass the duplicate-archive prompt for that download batch.
- **Generic Sorting Rules**: Overhauled the sorting rule system to be more flexible.
  - Users can now filter by various metadata fields: **Uploader**, **Title**, **Playlist Title**, **Tags**, **Description**, or **Duration**.
  - Added operators for filtering: **Is one of**, **Contains**, **Equals**, **Greater than**, and **Less than**.
  - Retained support for filtering by download type (All, Video, Audio, Gallery).
  - Existing uploader-based rules are automatically migrated to the new format.
- **Dynamic Subfolder Patterns**: Replaced the simple "Date-based Subfolders" checkbox with a powerful **Subfolder Pattern** field.
  - Users can define custom subfolder structures using tokens like `{upload_year}`, `{upload_month}`, `{uploader}`, `{title}`, etc.
  - Example: `{upload_year}/{uploader}` will sort files into `Target Folder\2025\Agadmator`.
  - Existing date-based rules are automatically migrated to the pattern `{upload_year} - {upload_month}`.
- **Playlist-Aware Sorting Rules**: Added playlist-specific sort targeting and playlist-name token insertion.
  - Added two new **Rule Applies To** options: **Video Playlist Downloads** and **Audio Playlist Downloads**.
  - Added a new subfolder insert token option: `%(playlist)s` to create dedicated folders per playlist name.
  - Sorting subfolder patterns now support both `{token}` and `%(token)s` styles, including `%(playlist)s`.
- **gallery-dl Support**: Added support for downloading image galleries using `gallery-dl`.
  - Added "Gallery" option to the Download Type dropdown in the Start tab.
  - Bundled `gallery-dl` binary.
  - Added `gallery-dl` update button in the Advanced Settings tab.
  - Added browser cookie support for `gallery-dl` in Advanced Settings.

### Changed
- **Faster playlist pre-expansion path**: Playlist expansion fallbacks no longer force `--yes-playlist` full extraction and now keep `--flat-playlist --lazy-playlist` to gather entry URLs/titles faster before worker enqueue.
- **Sorting token insert dropdown**: Replaced the sorting editor insert option `%(playlist)s` with `Album {album}` for subfolder pattern building.
- **Improved Gallery Validation**: Relaxed validation for gallery downloads to allow common gallery sites (Instagram, Twitter, etc.) even if simulation fails, as `gallery-dl` simulation can be unreliable due to auth requirements.
- **Gallery Download Parsing**: Enhanced file detection for `gallery-dl` downloads by parsing stdout for file paths and falling back to directory snapshots if needed.
- **Download Archive Always On**: Archive checks and writes are now always enabled (UI toggle is removed and config is enforced to `download_archive=True`).
- **Archive UI removed from Advanced tab**: Download archive now runs fully in the background with no archive toggle or archive controls shown to users.
- **SQLite-only archive persistence**: Archive records now persist only in `download_archive.db` with no `.txt` archive path usage.
- **Dependency management migrated**: Replaced `requirements.txt` with `pyproject.toml` (PEP 621 project metadata + dependencies).

### Deprecated
-

### Removed
-

### Fixed
- **Sorting date token simplification**: Removed sorting helper tokens `upload_year`, `upload_month`, and `upload_day`. Sorting date helpers now use release-date tokens only (`release_year`, `release_month`, `release_day`).
- **Sorting token cleanup**: Removed sorting support for `album_year` (UI insert option and token resolution).
- **Sorting legacy rule cleanup**: Removed legacy sorting-rule compatibility paths (`date_subfolders`, `audio_only`, legacy single-filter fields, and uploader-list fallback). Sorting now uses only `download_type`, `conditions`, and `subfolder_pattern`.
- **Progress bar early 100% + incorrect postprocessing status**: Active download parsing now ignores subtitle/auxiliary transfer percentages (for example `.vtt` subtitle fetches) until main media transfer starts, and postprocessing detection no longer treats generic "Extracting ..." lines as postprocessing (only true post-download steps such as `Extracting audio`).
- **Release date metadata targeting**: Switched default filename templates and Advanced template insert tokens from `upload_date` to `release_date` so date-based output naming uses release date metadata by default. Sorting date helper tokens now prefer `release_date` with fallback to `upload_date` for backward compatibility.
- **Embedded media date precedence**: Metadata writing now explicitly sets `meta_date` with strict fallback order `release_date` -> `release_year` -> `upload_date`, preventing corrupted `release_year` values from overriding valid date metadata.
- **GUI stutter during concurrent postprocessing**: Reduced main-thread repaint pressure by throttling duplicate progress updates and avoiding repeated progress-bar stylesheet resets during high-frequency yt-dlp/ffmpeg status output.
- **Playlist burst false `yt-dlp not available` failures**: Hardened `yt-dlp` availability checks to avoid transient failures under heavy concurrent playlist starts.
  - Added a lock-protected verification cache so many workers do not all run `yt-dlp --version` at once.
  - Increased verification timeout and reused the last-known-good check for brief timeout spikes.
  - Prevented temporary verification timeouts from immediately invalidating an otherwise working bundled `yt-dlp`.
- **Rate limit "None" handling**: Treat unlimited (`None`) as no throttle and prevent emitting invalid `--limit-rate None` in yt-dlp command args.
- **yt-dlp nightly option compatibility**: Removed unsupported `--no-input` usage from yt-dlp metadata/playlist pre-expansion commands to prevent immediate expansion/validation failures on newer yt-dlp builds.
- **Runtime concurrency above startup cap**: The Start tab now allows selecting up to 8 concurrent downloads for the current session while still persisting a maximum of 4 in config for the next app launch.
- **Temp subtitle file cleanup**: Added post-move cleanup for leftover subtitle sidecars (for example `.en.srt`) in `temp_downloads` when separate subtitle files are not enabled, preventing embedded-subtitle temp artifacts from being left behind.
- **HLS fragment progress no longer locks at 100% early**: Active download progress parsing now detects `frag X/Y` lines and prevents transient `100.0% ... (frag 0/N)` outputs from pinning the UI bar at 100% for the rest of the transfer.
- **Fixed Sorting Rule Album Detection**: Added fallback logic to use playlist title as the album name when the `album` metadata field is missing. This ensures `{album}` subfolder patterns work correctly for playlists (e.g., YouTube Music albums) that don't explicitly provide album metadata.
- **Enhanced Album Detection**: Added fallback to extract album metadata directly from downloaded files using `ffprobe` (checking both container and stream tags) if `yt-dlp` metadata is missing the album field. This improves sorting reliability for playlists where album info is embedded in the file but not in the JSON metadata.
- **Playlist Sorting Recognition**: Fixed an issue where downloads from expanded playlists were not recognized as playlist items by the sorting system. The application now internally tracks playlist context, ensuring that "Audio Playlist Downloads" and "Video Playlist Downloads" sorting rules are correctly applied without modifying metadata.
- **Failed-download list cleanup after successful retry**: URLs are now removed from the failed-download summary once a retry succeeds, so only currently failed downloads are shown.
- **Download archive repeat detection**: Duplicate downloads are now blocked before queueing by checking the archive directly in `DownloadManager`.
- **Archive key handling**: `ArchiveManager` now uses robust URL key generation (including YouTube `watch`, `shorts`, `live`, `embed`, and `youtu.be` forms), normalizes non-YouTube URLs, and avoids writing duplicate archive entries.
- **Archive duplicate UX**: When a URL is already archived, users are now prompted to either cancel or "Download Again" instead of being hard-rejected.
- **Playlist UI row collapsing**: Active Downloads now tracks widgets by unique item IDs instead of URL-only keys, so playlist entries always render as distinct rows even when URLs repeat.
- **Playlist expansion fallback regression**: Playlist expansion now parses and uses valid JSON output even when yt-dlp exits non-zero, retries YouTube Music `watch+list` links via canonical `youtube.com/playlist` URLs, and normalizes flat-playlist YouTube entries into per-video watch URLs.
- **Stuck playlist placeholder + sparse expansion fallback**: The "Preparing playlist download..." placeholder is now always replaced once expansion completes, and playlist expansion now includes a line-based `yt-dlp --print` fallback to recover per-item URLs when single-JSON extraction fails.
- **Playlist expansion auth parity**: Playlist pre-expansion now uses the same cookie and JavaScript runtime settings as normal downloads (`cookies_from_browser` and `--js-runtimes`), improving expansion reliability for protected YouTube/YouTube Music playlists.
- **Playlist pre-listing reliability**: Added entry-aware expansion and additional full/lazy `--print` fallbacks so playlist rows can be populated with per-item titles/URLs before downloads start on providers where flat JSON expansion fails.
- **Playlist worker fan-out fallback**: Added a final expansion fallback that runs yt-dlp with worker-equivalent args (`build_yt_dlp_args`) plus `--skip-download --print` so playlists that only work in full download context can still be pre-expanded into one worker per item.
- **Playlist all-mode enforcement**: When a playlist URL is detected and expansion still returns only one URL, the app now aborts queueing (with a clear error) instead of silently launching one playlist worker that downloads items sequentially under a single UI row.
- **Playlist status row cleanup**: After replacing the playlist placeholder with expanded item rows, the Active tab now removes any leftover status row like "Calculating playlist (... items)..." or "Preparing playlist download...".
- **Playlist preparation progress feedback**: Playlist pre-expansion now streams extraction progress to the placeholder row, updating the progress bar text with live status like `Extracting playlist item X/Y` (or a URL-based extracting message when total count is unavailable) so users can see activity during long playlist scans.

### Security
-

## [0.0.3] - 02-09-2026

### 02-10-2026
- Improved application update cleanup: The installer file is now automatically deleted after the update process completes, preventing clutter in the temporary downloads folder.
- Fixed logging initialization for installed builds by routing logs to a user-writable directory (AppData/LocalAppData) when the app directory is not writable.

### 02-09-2026
- Updated the app self-update flow to avoid browser dependency and apply updates directly from the UI.
- Unified update asset selection to prefer installer/MSI or portable executables and added safe executable swap handling for non-installer builds.
- Downloads now target the configured temporary directory when available, falling back to a system temp directory if unset.
- Build: Adjusted the PyInstaller spec to emit onedir output for NSIS packaging.

## [0.0.2] - 02-08-2026

### 02-08-2026
- Fixed a missing `time` import in `core/yt_dlp_worker.py` that caused MP3 playlist downloads to fail with `name "time" is not defined`.
- Normalized bundled/system binary paths to avoid mixed path separators in Windows error messages.
- Added detailed stderr/stdout to yt-dlp verification failures to make the root cause visible.
- Fixed duplicate redownload confirmation by centralizing the archive prompt in the main window.

### 02-08-2026
- **Fixed metadata embedding for videos**: Added `--embed-metadata` and `--embed-thumbnail` flags to video downloads (previously only applied to audio).
- **Capped max concurrent downloads at 4 on app startup**: While users can temporarily set higher concurrency (up to 8) in the UI, any value above 4 reverts to 4 when the app restarts. This prevents users from accidentally launching with overly aggressive concurrency.
- **Added "View Formats" feature**: Users can now select "View Formats" from the Download Type dropdown to inspect all available download formats for a URL without downloading. This runs `yt-dlp -F <URL>` and displays results in a dialog.
- **Dynamic Download button text**: The main Download button now changes its label based on the selected Download Type (e.g., "Download Video", "Download Audio", "View Formats") to provide clear user feedback.
- **Added comprehensive mouseover tooltips**: All UI elements now have helpful tooltips explaining their purpose, including buttons, dropdowns, checkboxes, and labels across all three tabs (Start, Active, Advanced).
- **Early unsupported-URL detection**: Added a quick background validation step before creating Active Downloads UI elements. If `yt-dlp` cannot quickly extract metadata for a URL, the app will report the URL as unsupported and will not create the download UI entry, preventing wasted UI artifacts and faster user feedback.
- **Extractor index & host heuristics**: At startup the app now attempts to build a local extractor index (using the `yt_dlp` python module when available) and uses it to rapidly determine whether a host is supported. This reduces latency for common hosts like YouTube by avoiding the slower metadata probes.

### 02-08-2026
- **Implemented Tiered URL Validation**: Refined the early supported-URL detection into a two-tier system to balance speed and accuracy.
- **Tier 1 (Fast-Track)**: The app now performs an immediate string/regex check for high-traffic domains (e.g., `youtube.com`, `youtu.be`, `music.youtube.com`). These are accepted instantly to provide zero-latency UI feedback.
- **Tier 2 (Metadata Probe)**: For less-common domains, the app initiates a background `yt-dlp --simulate` call. The "Active Download" UI entry is only generated if this probe confirms the URL is a valid target.
- **Rationale**: This hybrid approach eliminates the "verification lag" for the most frequent use cases (YouTube) while preventing the UI from being cluttered with invalid or unsupported URLs for niche sites.

### 02-08-2026
- **Configurable Output Filename Pattern**: Added a new GUI textbox in the Advanced Settings tab to allow users to define the output filename pattern using `yt-dlp` template variables.
- **Default**: `%(title)s [%(uploader)s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s`
- **Validation**: Basic validation ensures balanced parentheses and brackets before saving.
- **Reset**: Users can easily revert to the default pattern.
- **Integration**: The `DownloadManager` now pulls this pattern from the configuration instead of using a hardcoded string.

### 02-08-2026
- **Fixed ConfigParser Interpolation Error**: Disabled interpolation in `ConfigManager` to prevent `configparser` from attempting to interpret `yt-dlp` template variables (like `%(title)s`) as configuration references. This fixed a crash on startup.

### 02-08-2026
- **Grouped Advanced Settings into categories**: The Advanced tab now clusters options into labeled sections (e.g., Configuration, Authentication & Access, Output Template, Download Options, Media & Subtitles, Updates, Maintenance) so users can find settings faster.
- **Implemented Application Self-Update Mechanism**: Added logic to check for updates from GitHub Releases, download the update, and apply it.
- **Update Check**: Queries GitHub API for the latest release tag.
- **Download**: Downloads the update asset (exe/msi) to a temporary location.
- **Apply**: If it's an installer, launches it. If it's a binary replacement, uses a batch script to swap the executable and restart.
- **UI Integration**: Added a prompt in the main window when an update is available, with options to update now or view the release page.
- **Added chapter embedding toggle**: New Advanced Settings checkbox (default on) to pass `--embed-chapters` to yt-dlp when available.

### 02-08-2026
- **Added Global Download Speed Indicator**: Implemented a real-time download speed indicator at the bottom of the main application window.
- **Implementation**: Uses `psutil.Process().io_counters().read_bytes` to measure the total I/O read throughput of the application process.
- **UI Update**: Added a `QLabel` to the bottom of `MediaDownloaderApp` to display the total speed.
- **Aggregation**: Implemented a timer in `MediaDownloaderApp` to calculate the speed difference every second.
- **Rationale**: Provides a more accurate and simpler measure of actual data throughput compared to parsing `yt-dlp` stdout, and avoids per-file tracking complexity.
- **Ensured ffmpeg fallback for yt-dlp**: `yt-dlp` now receives an explicit `--ffmpeg-location` that prefers system `ffmpeg/ffprobe` when both are present, and otherwise falls back to the bundled binaries to ensure merging and post-processing work on clean systems.

### 02-08-2026
- **Added Theme Selection**: Added a dropdown in Advanced Settings to switch between "System", "Light", and "Dark" themes.
- **Implementation**: Updates `main.pyw` to apply the selected theme on startup.
- **Dark Mode**: Uses `pyqtdarktheme` if available, otherwise falls back to system style.
- **Persistence**: Saves the user's choice in `settings.json`.
- **Dynamic Switching**: Theme changes are applied immediately without requiring a restart.
- **Robustness**: Added checks for `setup_theme` vs `load_stylesheet` to support different versions of `pyqtdarktheme` or compatible libraries.
- **System Theme Detection**: Added Windows registry check to correctly detect system theme when "System" (auto) is selected, fixing an issue where it defaulted to dark mode.

### 02-08-2026
- **Fixed Global Download Speed Indicator**: Modified the download speed indicator to include I/O from child processes.
- **Implementation**: The speed calculation now iterates through the main process and all its children (`psutil.Process().children(recursive=True)`), summing their `io_counters` to get a total I/O throughput.
- **Rationale**: This provides a more accurate download speed measurement, as `yt-dlp` runs in separate subprocesses whose I/O was not previously being tracked.

### 02-08-2026
- **Fixed Global Download Speed Indicator (Again)**: The previous fix for the download speed indicator was flawed and caused it to display `-- MB/s`. The calculation logic has been rewritten to be more robust and now correctly sums the I/O from the main process and all its children.
- **Implementation**: The `_get_total_io_counters` function now simply sums the `read_bytes` from the main process and all its children, with proper error handling for terminated processes.
- **Rationale**: This simpler implementation is more resilient and provides an accurate total download speed.

### Fixed
- Fixed PyInstaller build hang on PyQt6.QtGui hook processing (PyInstaller 6.18.0 issue)
  - Solution: Created custom minimal PyQt6.QtGui hook to bypass problematic default hook
  - Added `hooks/` directory with simplified PyQt6.QtGui hook
  - Downgraded PyInstaller from 6.18.0 to 6.17.0 for better stability
  - Build now completes successfully and exe launches correctly
- Fixed critical app crash on startup caused by corrupted method merging in `main_window.py`
- Fixed unhandled exception in version fetch from daemon thread (disabled for now pending future refactor)
- Added robust error handling around signal emission in background threads
- Do not auto-create `temp_downloads` or set default output directory on first run; leave paths unset until user selects them

## [0.0.1] - 02-02-2026

### Added
- Initial release of MediaDownloader
- PyQt6-based GUI for downloading media via yt-dlp
- Support for 1000+ websites (YouTube, TikTok, Instagram, etc.)
- Playlist detection and expansion
- Concurrent download management with user-configurable limits (capped at 4 on startup)
- Advanced download options:
  - Audio/video quality selection
  - Format filtering by codec
  - SponsorBlock integration for automatic segment removal
  - Filename sanitization and customization
- Metadata and thumbnail embedding for videos and audio
- Browser cookie integration for age-restricted content
- Optional JavaScript runtime support (Deno/Node.js) for anti-bot challenges
- GitHub-based auto-update system:
  - Automatic release checking on app startup
  - Silent installer-based updates via NSIS
  - Manual update check button in Advanced Settings
  - Configurable auto-check toggle (persisted to settings)
  - Changelog display before updating
- Responsive UI with comprehensive tooltips
- File lifecycle management:
  - Download to temporary directory
  - File stability verification
  - Move to completed downloads directory
- Download archive tracking (prevent re-downloads)
- Robust error handling with user-friendly messages
- Comprehensive logging to file and console
- Enforced output directory selection on first run
- In-app yt-dlp version management:
  - Manual update button with stable/nightly channel selection
  - Automatic version display
- NSIS-based Windows installer for standalone distribution
- PyInstaller bundled with all dependencies (yt-dlp, ffmpeg for Windows/Linux/macOS)

### Fixed
- Signal handler connection for file move operations (worker finished signal now properly invokes handler)
- Snapshot fallback file detection by cleaning temp directory on test start
- Early URL validation to prevent wasted UI artifacts for unsupported hosts

### Technical Details
- Built with PyQt6 for responsive, cross-platform UI
- Uses yt-dlp for reliable media downloading
- FFmpeg for video/audio processing and metadata embedding
- Background thread workers for non-blocking I/O operations
- Modular architecture separating UI, core logic, and utilities
- Git-ignored `/bin/` directory with automated binary download script (`download_binaries.ps1`)
- NSIS installer supporting silent installation (`/S /D=<path>` flags) for seamless updates
- Semantic versioning in `core/version.py` as single source of truth

## Future Roadmap

- [ ] Automatic nightly builds and releases
- [ ] Rollback to previous version option
- [ ] Delta updates (download only changed files)
- [ ] Silent automatic updates (no prompt, happens in background)
- [ ] Custom video/audio post-processing
- [ ] Subtitle/caption download integration
- [ ] Search and download by query
- [ ] Download statistics and analytics dashboard
- [ ] Cross-platform support (native builds for macOS/Linux)
- [ ] Download statistics and analytics dashboard
- [ ] Cross-platform support (native builds for macOS/Linux)
