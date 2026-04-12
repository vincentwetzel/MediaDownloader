# Changelog

All notable changes to MediaDownloader will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - YYYY-MM-DD

### Added
- **Centered progress text on progress bar**: The download percentage, file sizes, speed, and ETA are now painted directly in the center of the progress bar via a custom `ProgressLabelBar` widget, replacing the separate details label below it.
- **Auto version bump on push**: A git pre-push hook (`.git/hooks/pre-push`) automatically increments the patch version in `CMakeLists.txt` on every push. The hook amends the latest commit so the pushed commit always has the new version. Skip with `SKIP_VERSION_BUMP=1 git push`.
- **Centralized app versioning system**: Version is now defined once in `CMakeLists.txt` (`project(VERSION x.y.z)`) and automatically propagated to the generated `version.h`, Windows `.rc` file (file properties version), window title, and the update checker. Bump the version in one place and everything updates.
- **Immediate queue UI feedback**: Downloads now appear instantly in the Active Downloads tab without waiting for playlist expansion:
  - Gallery downloads appear immediately with "Queued" status
  - Video/audio downloads show "Checking for playlist..." during expansion, then update to "Queued"
  - Playlists replace the placeholder with individual track items
  - Queue state persistence deferred via `Qt::QueuedConnection` to prevent GUI blocking
- **Color-coded progress bars**: Download progress bars now use color-coding to provide clear visual feedback on download state:
  - Colorless/default when queued or initializing
  - Light blue (#3b82f6) while actively downloading
  - Teal (#008080) during post-processing (merging, metadata embedding)
  - Green (#22c55e) when download is fully completed
- **Detailed progress information**: Download items now display comprehensive, real-time progress details comparable to command-line yt-dlp:
  - Status label shows current download stage (e.g., "Extracting media information...", "Downloading 2 segment(s)...", "Merging segments with ffmpeg...", "Verifying download completeness...", "Applying sorting rules...", "Moving to final destination...")
  - Progress details below the bar show: downloaded/total size, download speed, and ETA (e.g., "15.3 MiB / 45.7 MiB  •  Speed: 2.4 MiB/s  •  ETA: 0:12")
- **Single Instance Enforcement**: The application now ensures only one instance can run at a time using `QSystemSemaphore` and `QSharedMemory`.
- **C++ Port**: Initial release of the C++ version of MediaDownloader.
  - Re-implemented the entire application using C++ and Qt 6.
  - Designed as a drop-in replacement for the Python version (v0.0.10).
  - Maintains full compatibility with existing `settings.ini` and `download_archive.db`.
  - Improved performance and reduced memory usage compared to the Python version.
  - Native look and feel on Windows.
- **Clipboard auto-paste domain list**: Added app-directory `extractors.json` loading for Start tab clipboard URL auto-paste.
- **Auto-paste on focus toggle**: Added an Advanced Settings option to auto-paste clipboard URLs when the app is focused or hovered.
- **Aria2c RPC Daemon Integration**: Replaced standard `yt-dlp` download execution with a background `aria2c` daemon, enabling massively faster multi-connection segment downloads.
- **Comprehensive UI Tooltips**: Enforced a requirement that *all* GUI elements must have hover hints (tooltips) and added missing tooltips across the application.
- **Asynchronous Engine**: Fully decoupled `yt-dlp` metadata extraction and `ffmpeg` post-processing into their own non-blocking workers to prevent UI freezes.
- **Automatic Extractor Updates**: `YtDlpUpdater` now automatically regenerates `extractors_yt-dlp.json` using `yt-dlp --list-extractors` to keep auto-paste heuristics perfectly in sync with the binary.
- **Playable Thumbnail Previews**: Added the ability to click on thumbnail previews in the Active Downloads tab to instantly open the completed media file in the OS default player.
- **PlaylistExpander full command construction**: `PlaylistExpander` now uses `YtDlpArgsBuilder` to construct the full yt-dlp command (including `--js-runtimes deno:...`, `--cookies-from-browser`, `--ffmpeg-location`) so playlist expansion matches the actual download configuration. Resolves `n challenge solving failed` errors during playlist expansion.
- **YtDlpWorker diagnostic logging**: Added comprehensive debug logging for process state changes, stderr/stdout data received, and progress parsing to aid in diagnosing download issues.
- **Download thumbnail previews**: `DownloadItemWidget` now displays a thumbnail preview on the left side of each download item's progress bar. Thumbnails are loaded from the `thumbnail_path` field emitted by `YtDlpWorker` when yt-dlp converts the thumbnail during the download process.
- **Logging to AppData**: Log files are now stored in the user's AppData configuration directory (`%LOCALAPPDATA%\MediaDownloader\MediaDownloader.log` on Windows), alongside `settings.ini`, instead of the application directory. This ensures logs are preserved across application updates and installations.
- **Log rotation/cycling**: Implemented automatic log rotation with a maximum of 5 log files. When the current log exceeds 2 MB, it is rotated (`.log` → `.log.1` → `.log.2`, etc.), and the oldest log is automatically deleted. This prevents unbounded disk growth while preserving recent diagnostic history.
- **Sorting rules metadata fix**: Fixed sorting rules not matching because `uploader` metadata was lost after `readInfoJsonWithRetry` cleared the info.json path. `YtDlpWorker` now caches the full metadata (`m_fullMetadata`) when info.json is successfully parsed, ensuring all fields (including `uploader`, `channel`, `tags`, etc.) are available for sorting rule evaluation when the download completes.
- **Smart URL download type switching**: The Start tab now automatically detects which extractor (yt-dlp or gallery-dl) supports a pasted URL and switches the download type accordingly. If the URL is yt-dlp exclusive and "Gallery" is selected, it switches to "Video". If gallery-dl exclusive, it switches to "Gallery". If both or neither support it, no change is made.
- **Binary path caching**: `ProcessUtils` now caches resolved binary paths after the first lookup to avoid repeated PATH scans. The cache is automatically cleared when binaries are installed or overridden via the Advanced Settings Binaries page.
- **Expanded gallery-dl template tokens**: The Output Templates page now includes a comprehensive list of gallery-dl format tokens (including `{category}`, `{user}`, `{subcategory}`, `{author[...]}`, `{date}`, `{post[...]}`, `{media[...]}`, etc.) for easier filename template creation.
- **Flattened AppData directory structure**: Removed duplicate organization name in `main.cpp` so app data is stored in `%LOCALAPPDATA%\MediaDownloader\` instead of `%LOCALAPPDATA%\MediaDownloader\MediaDownloader\`.

### Fixed
- **Archive duplicate detection**: Added pre-enqueue duplicate check in `DownloadManager::enqueueDownload()` that consults `ArchiveManager::isInArchive()` before allowing a download into the queue. Respects the "Override duplicate download check" toggle. Strips all query parameters from non-YouTube URLs during normalization so the same content accessed via different URLs is correctly detected as a duplicate. Replaced scattered temporary `ArchiveManager` instances with a single shared member.
- **Sorting Rule dialog condition text entry size**: Set `CONDITION_VALUE_INPUT_HEIGHT` constant to 100px applied via `setFixedHeight()` for consistent text entry sizing across all conditions.
- **Sorting Rule dialog overflow**: Fixed condition text entry boxes exceeding dialog bounds. Increased dialog minimum size to 650x500.
- **Download queue immediate start fix**: Fixed downloads failing to start after immediate queue UI feedback implementation. Items are now correctly added to the download queue before playlist expansion, and `saveQueueState`/`startNextDownload` are properly invokable for deferred execution.
- **Log cycling**: Changed from size-based rotation to one log file per run with timestamp in filename (`MediaDownloader_YYYY-MM-dd_HH-mm-ss.log`). Automatically keeps only the 10 most recent logs.
- **Toggle switches not displaying correctly**: Fixed `ToggleSwitch` widget not updating its visual position when `setChecked()` was called with `QSignalBlocker`. The `paintEvent()` now ensures the handle offset matches the checked state.
- **gallery-dl executable not detected after pip install**: `GalleryDlWorker` now uses `ProcessUtils::findBinary()` to resolve the gallery-dl executable, properly detecting system PATH installations (e.g., via pip) instead of only checking bundled paths.
- **gallery-dl output template handling**: Simplified gallery-dl template handling. The `-f` flag natively supports path templates with `/` separators, so no splitting is needed.
- **gallery-dl default output template**: Changed the default gallery-dl filename pattern from `{author[name]}/{id}_{filename}.{extension}` to `{category}/{id}_{filename}.{extension}` for better organized downloads that work across all extractors.
- **gallery-dl progress bar fix**: Fixed the gallery-dl download progress bar not updating. The worker now correctly parses the file path output from gallery-dl and displays the amber "downloading" color on the progress bar.
- **gallery-dl single file move fix**: Fixed gallery downloads that produce a single file instead of a subdirectory. The app now correctly handles both files and directories when moving gallery downloads to their final destination.

### Changed
- **Architecture**: Switched from Python/PyQt6 to C++/Qt6.
- **Build System**: Switched from PyInstaller to CMake/MSVC.
- **Extractor loading**: Replaced runtime `yt-dlp --eval` attempts with app-directory `extractors.json` loading.
- **External binaries workflow**: The Advanced Settings binaries page now reports per-dependency status, preserves manual browse overrides, and offers package-manager or manual-download install actions for discovered/missing tools.

### Removed
- **Unused SSL Libraries**: Removed `libssl-3-x64.dll` and `libcrypto-3-x64.dll` from the `bin/` directory to reduce distribution size, as Qt 6 natively uses the Windows SChannel backend.

### Fixed
- **Qt6 package discovery in CLion/Ninja builds**: `CMakeLists.txt` now seeds `CMAKE_PREFIX_PATH` from `Qt6_DIR`/`QT_DIR`/`QTDIR` environment variables and common Windows installs such as `C:\Qt\6.*\msvc2022_64`, so `find_package(Qt6 ...)` works even when the IDE does not inherit a Qt kit path.
- **Unbundled binary path consistency**: `BinariesPage`, `StartTab`, startup checks, and `YtDlpWorker` now resolve the same hyphenated binary config keys and use shared runtime detection instead of assuming bundled-only executables.
- **Native yt-dlp progress parsing**: `YtDlpWorker` now parses native yt-dlp progress lines with approximate/unknown totals and keeps aria2-compatible progress updates, restoring download percentages and speed reporting when aria2 is unavailable or disabled.
- **UI/Layout Resizing**: Fixed an issue where the main window could not be resized or snapped to half the screen due to rigid horizontal minimum width constraints in the footer, Start tab, and inactive background tabs.
- **Misplaced runtime format controls on Start tab**: Removed the accidental per-download "Max Resolution", "Video Codec", and "Audio Codec" override dropdowns from `StartTab`; runtime video/audio selection is now documented and routed through Advanced Settings-driven dialogs instead.
- **Mixed-mode Advanced Settings confusion**: Video and Audio Advanced Settings now treat `Quality = Select at Runtime` as a full runtime-selection mode and hide the remaining format-default controls on that page so users do not configure both workflows at once.
- **Runtime format multi-enqueue behavior**: Fixed a compilation and logic bug in the runtime format picker. It now correctly supports selecting multiple formats from the prompt and treats each checked item as its own distinct queued download.
- **Runtime format prompt ownership**: Runtime video/audio quality, codec, and stream selection is now handled from `DownloadManager`/`MainWindow` rather than competing with Start-tab URL probing logic.
- **Restore Defaults Safety**: "Restore Defaults" in Advanced Settings no longer permanently deletes dynamically created Sorting Rules or resets the user's selected UI theme, output templates, and external binary paths.
- **Advanced Settings toggle switches snapping back after click**: The custom `ToggleSwitch` control no longer toggles twice on mouse release, so slider-style settings in Advanced Settings now stay in the selected state and persist correctly after a single click.
- **Redundant startup config saves**: Startup no longer re-runs `ConfigManager::loadConfig()` from `StartTab`, and the Start tab lock-setting checkboxes no longer fire save-on-load handlers during initial UI hydration. Configuration saves now only occur from those paths when a setting actually changes.
- **Qt Debug runtime plugin deployment on Windows**: CMake now deploys both release/debug Qt runtime DLLs and plugin variants (`platforms`, `sqldrivers`, `tls`) so Debug builds correctly load `QSQLITE` and TLS backends (`qsqlited.dll`, `qschannelbackendd.dll`) instead of failing with "Driver not loaded" / "No TLS backend is available".
- **Active download thumbnail previews not rendering in C++ builds**: CMake now deploys the Qt `imageformats` plugins needed for converted thumbnails (`qjpeg`/`qpng` in addition to existing `qwebp`/`qico`), and `DownloadItemWidget` now uses `QImageReader` with retry-aware diagnostics so per-download artwork can render reliably beside the progress bar.
- **Zombie background processes**: Fixed an issue where closing the application or canceling a download would leave `aria2c.exe`, `ffmpeg.exe`, or `yt-dlp.exe` running invisibly in the OS background.
- **FFmpeg WebM and Audio Subtitle Crashes**: Prevented FFmpeg from crashing permanently when attempting to mux `srt` subtitles into `.webm` (now strictly uses `webvtt`) or audio-only containers.
- **App process lingering after window close**: Closing the main window now exits the application instead of minimizing to tray and continuing in the background.
- **Playlist UI memory leaks**: Captured transient `QObject::sender()` objects locally to prevent memory leakage and data loss during synchronous playlist message box prompts.
- **Unicode/special-character output path handling on Windows**: Download finalization no longer relies on `cmd /c move`. The app now forces UTF-8 process/output handling (`PYTHONUTF8`, `PYTHONIOENCODING`, `yt-dlp --encoding utf-8`), preserves Unicode in logger output, and uses Qt-native move/copy fallback so files with characters like `？` or emoji move reliably from `--print after_move:filepath` output.

### Security
-

---

## [0.0.10] - 02-18-2026 (Python Version)

### Added
- **Active download thumbnail previews**: The Active Downloads list now shows per-item thumbnail previews (when available) to the left of each title/progress row.
- **Audio playlist track-number tagging**: Playlist expansion now propagates `playlist_index` per entry, and audio playlist downloads write `track`/`tracknumber` tags so player ordering matches playlist order.
- **Developer Discord footer link**: Added a Discord icon button beside "Contact Developer" at the bottom of the main window; clicking opens `https://discord.gg/NfWaqKgYRG` and shows tooltip text "Developer Discord".

### Changed
- **Rotating application logs**: Switched the main file logger to size-based rotation (`MediaDownloader.log`, 10 MB per file, 5 backups) to prevent unbounded log growth.

### Fixed
- **Sorting subfolder token sanitization**: Sorting subfolder token values (for example `{album}`) are now sanitized before path assembly so illegal path characters like `/` and `\` are replaced with `_` instead of creating unintended nested folders or truncating names.
- **Thumbnail signal chain for UI rendering**: Wired `DownloadWorker` thumbnail events through `DownloadManager` to `ActiveDownloadsTab`, ensuring downloaded thumbnail images are actually rendered in the GUI.
- **Queued-item thumbnail timing**: Added early thumbnail prefetch during metadata preloading so queued downloads can show thumbnails before transfer starts.
- **Thumbnail preview persistence**: Thumbnail images used by the Active Downloads UI are now stored in a session-only temp cache and cleaned up automatically on app exit.
- **Audio active-thumbnail framing**: Active Downloads thumbnail previews now center-crop audio-only artwork to a square before display, matching the existing high-quality thumbnail conversion behavior used during download postprocessing.
- **OPUS artwork embedding regression**: The worker now skips the custom ffmpeg attached-pic remux path for `.opus` outputs and preserves yt-dlp's native OPUS artwork embedding behavior.
- **Playlist metadata continuity across expansion fallbacks**: All playlist expansion paths now preserve per-entry `playlist_index`/`playlist_count` metadata so downstream worker logic receives stable ordering data.
- **OPUS playlist track tagging reliability**: Playlist `track`/`tracknumber` tagging for `.opus` outputs now uses in-place tag updates that avoid artwork-stripping remux paths.
- **OPUS artwork loss after playlist track tagging**: `.opus` playlist track tags are now written in-place with `mutagen` instead of ffmpeg remux, preserving embedded artwork while still applying `track`/`tracknumber`.
- **Playlist track tag formatting**: Playlist `track`/`tracknumber` values are now zero-padded for single-digit indices (for example, `01`..`09`) to improve ordering consistency in players.
- **Playlist audio filename ordering**: Audio playlist downloads are now renamed on move to include a zero-padded playlist index prefix (`NN - `), for example `01 - <original name>.opus`.
- **Audio title truncation on dotted movement names**: Active Downloads title cleanup now strips extensions only for known media/container suffixes, preventing titles like `I. Molto allegro...` from being cut at the first movement period.

## [0.0.9] - 02-17-2026

### Added
-

### Changed
-

### Deprecated
-

### Removed
-

### Fixed
- **Download percentage visibility during active transfers**: Fixed progress parsing so `.part` destination lines are treated as primary media transfer context (not auxiliary files), restoring visible download percentages during yt-dlp/aria2 runs.
- **aria2 progress text stability**: Ignored aria2 noise lines without numeric progress (`[#...]`, `FILE: ...`, separator/summary lines) in active download rendering so they no longer overwrite the last shown percentage.
- **HLS fragment progress accuracy with aria2**: Added fragment-based fallback progress (`Total fragments` + `.part-FragN`) so long HLS downloads no longer stall around `<1%` when aria2 byte summary percentages are unstable.
- **Repeat-download row reuse in Active Downloads**: Starting a download for a URL that already has a terminal row (`Done`, `Cancelled`, or `Error`) now creates a new Active Downloads widget instead of reusing the old one.

### Security
-

## [0.0.8] - 02-16-2026

### Added
- **Version in Title Bar**: The application window title now includes the version number (e.g., "Media Downloader v0.0.8").
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
