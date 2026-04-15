# LzyDownloader C++ Port TODO

## In Progress

### Phase 14: Unbundled Binaries & Dependency Management
- [x] **Project Cleanup**:
  - [x] Remove the `bin/` directory and all bundled executables from the source repository. **COMPLETED**: The `bin/` directory was already not tracked in git. Removed fallback code from `ProcessUtils::resolveBinary()` that checked for bundled binaries.
  - [x] Update `CMakeLists.txt` and deployment scripts (`UPDATE_AND_RELEASE.md`) to reflect the unbundled approach. **COMPLETED**: Updated `SETTINGS.md`, `README.md`, and `AGENTS.md` to remove references to `bin/` directory. Binary resolution now only checks: (1) User-configured path, (2) System PATH, (3) User-local install locations.
  - [x] Remove binary update scripts (e.g., `update_binaries.ps1`). **COMPLETED**: Deleted `update_binaries.ps1` from repository.

### Error Handling
- [x] **Private Videos**: Display an error popup window explaining to the user that the video is private (e.g., `https://www.youtube.com/watch?v=V47ahjtbcaA`). **IMPLEMENTED**: YtDlpWorker now parses stderr for "private" errors and emits `ytDlpErrorDetected` signal, which triggers a user-friendly QMessageBox popup via DownloadManager and MainWindow.
- [x] **Unavailable Videos**: Display an error popup window explaining to the user that the video is unavailable (e.g., `https://www.youtube.com/watch?v=ZFVk4kK6fKs`). **IMPLEMENTED**: Same infrastructure as private videos, also detects "unavailable", "no longer available", and "does not exist" error patterns. Extended to also detect geo-restricted, members-only, and age-restricted videos.

### User Prompts & Validation
- [x] **Livestream Implementation**: 
  - [x] **Detection**: Check `info.json` metadata to determine if `is_live` is true before queuing.
  - [x] **Livestream Settings UI**: Add a "Livestream Settings" section beneath "Audio Settings" in the Advanced Settings tab.
    - [x] `Record from beginning`: Toggle for `--live-from-start` vs `--no-live-from-start`.
    - [x] `Wait for video`: Toggle and min/max spinboxes to construct `--wait-for-video MIN-MAX`.
    - [x] `Download As`: Format enforcement (MPEG-TS or MKV), preferring MPEG-TS (`--hls-use-mpegts`).
    - [x] `Use .part files`: Toggle for the `--part` and `--no-part` flags.
    - [x] `Quality`: Dropdown specifically targeting livestream resolutions.
    - [x] `Convert To`: Video codec extensions linked to FFmpeg post-processing options.
  - [x] **Argument Builder**: Update `YtDlpArgsBuilder` to route to Livestream parameters when the flag is detected.
  - [x] **Interactive Prompt**: When a scheduled livestream is detected, prompt the user to wait for it.
  - [x] **Worker Resilience**: Parse livestream indeterminate progress output and handle "Waiting for video" lines.

### Bug Fixes
- [x] **Single-Item Playlist Double Download**: The URL `https://music.youtube.com/watch?v=Bkh2BJ49DmQ&list=OLAK5uy_n3IQt8nfMSp3Xlma2hMsvKAyHBmBwk5Is` (a playlist with 1 item) downloads the audio file twice and sorts it to two different places. It should only download once and correctly trigger the "Audio Playlist Downloads" rule instead of the single "Audio Downloads" rule.

## Completed

### Core Features (Phases 1-10)
- UI, download logic, settings, archive, playlists, sorting, advanced settings
- Global download speed indicator, video quality warning, clean display title
- Retry/resume signals, single instance enforcement

### Stability & Deployment (Phases 11-12)
- NSIS installer, UI enhancements, advanced settings reorganization
- Restore defaults safety, cancel downloads in queue, download stats display
- Qt plugin deployment reliability, thumbnail plugin deployment
- Unicode filename move reliability, canonical `settings.ini` cleanup
- Extractor domain list maintenance, thumbnail previews, Qt SDK auto-discovery

### Future Enhancements (Phase 13)
- Cancel/retry buttons, interface languages, runtime format selection
- Backup download queue, clear completed downloads, cancel all downloads
- Pause/resume downloads, download queue ordering
- Generate `folder.jpg` for audio playlists, subtitle selection improvements
- Settings storage location, crop audio thumbnails, split output templates
- Expand output template fields, `--split-chapters` support, geo-verification proxy

### Settings Architecture (Phase 16)
- Simplified `ConfigManager`, removed canonicalization loops and legacy fallbacks
- Streamlined `get()`/`set()`, preserved factory defaults and safe reset

### Binary Discovery (Phase 14 partial)
- `BinaryFinder` utility, binary path caching, gallery-dl PATH detection
- Dynamic UI adaptation, external binaries settings tab
- Flexible progress parsing (native yt-dlp + aria2c), optional dependencies

### UI/UX Enhancements (Phase 15)
- Toggle switch visual fix, smart URL download type detection
- gallery-dl output template fixes, progress bar updates, move logic fixes
- **Open folder buttons on Active Downloads tab**: **IMPLEMENTED**: Added "Open Temporary Folder" and "Open Downloads Folder" buttons to the Active Downloads tab toolbar, duplicating the functionality from the Start tab for easier access during downloads.
- **External Downloader dropdown in Advanced Settings**: **IMPLEMENTED**: Changed "Download Options -> External Downloader (aria2)" from a ToggleSwitch to a QComboBox with two options: "yt-dlp (default)" and "aria2c". The setting is automatically hidden if aria2c is not installed/discovered. Default changed to yt-dlp for consistency with unbundled binary model.

### Bug Fixes (Phase 14)
- **Progress Bar Completion Issue**: YtDlpWorker now emits a 100% progress update before the finished signal, ensuring the UI progress bar reaches 100% and turns green.
- **GUI Counters Update**: DownloadManager now tracks errors with `m_errorDownloadsCount` and emits it in the `downloadStatsUpdated` signal.
- **Error Counter**: Added "Errors: X" label that tracks downloads that failed at any stage (worker failure, metadata embedding, file moves, playlist expansion, gallery downloads).
- **Progress Parsing Verification**: Verified that the app correctly parses output from both yt-dlp and aria2c with dedicated parsing functions for each downloader.

### Recent Additions
*(Moved to CHANGELOG.md)*
