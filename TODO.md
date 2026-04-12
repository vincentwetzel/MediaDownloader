# MediaDownloader C++ Port TODO

## In Progress

### Phase 14: Unbundled Binaries & Dependency Management
- [ ] **Project Cleanup**:
  - [ ] Remove the `bin/` directory and all bundled executables from the source repository.
  - [ ] Update `CMakeLists.txt` and deployment scripts (`UPDATE_AND_RELEASE.md`) to reflect the unbundled approach.
  - [ ] Remove binary update scripts (e.g., `update_binaries.ps1`).

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

### Recent Additions
- **Color-coded progress bars**: Colorless (queued), light blue (downloading), teal (post-processing), green (completed)
- **Detailed progress display**: Status label showing download stage, centered progress text on bar with percentage, sizes, speed, and ETA
- **Immediate queue UI feedback**: Downloads appear instantly without waiting for playlist expansion
- **Centralized versioning system**: Single source of truth in `CMakeLists.txt` → generated `version.h`, `.rc` file, window title, updater
- **Auto version bump on push**: Git pre-push hook increments patch version automatically
- **Archive duplicate detection**: Pre-enqueue check using `ArchiveManager::isInArchive()` with override toggle support
- **Download completion with destination path**: Success messages now show the full file path
- **Smooth scrolling in SortingRuleDialog**: Replaced `QListWidget` with `QScrollArea` for pixel-level scrolling
- **Log cycling fix**: One log file per run with timestamp in filename, automatic cleanup of old logs
