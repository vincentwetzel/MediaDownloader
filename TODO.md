# MediaDownloader C++ Port TODO

## Phase 1-8: Core Implementation & Parity (Complete)
- [x] All major features including UI, download logic, settings, archive, playlists, sorting, advanced settings, and polish items have been implemented.

## Phase 9: Final Feature Parity (Complete)
- [x] **Global Download Speed Indicator**: Implemented.
- [x] **Video Quality Warning**: Implemented.
- [x] **Clean Display Title**: Implemented.
- [x] **Connect Retry/Resume Signals**: Implemented.

## Phase 10: Stability & Robustness (Complete)
- [x] **Single Instance Enforcement**: Implemented using `QSystemSemaphore` and `QSharedMemory` to prevent multiple application instances.

## Phase 11: Finalization & Deployment (To Do)
- [ ] **Installer**: Create and test the NSIS installer.
- [ ] **Testing**:
    - [ ] Thoroughly test all features.
    - [ ] Verify seamless update from the Python version.
- [x] **UI Enhancements**:
    - [ ] Dynamically populate video quality and codec options from `yt-dlp` output.
    - [x] Expanded "Rate Limit" dropdown options to include a wider range of slower speeds.
    - [x] **Advanced Settings Tab Reorganization**: Implemented new organization, auto-saving for most settings, dedicated save button for output template with validation, subtitle language combo box with full names, and removal of `yt-dlp` update channel selection.
    - [x] **Start Tab Settings Relocation**: Moved "Override duplicate download check" and "Enable SponsorBlock" to StartTab.
    - [x] Provides immediate feedback if a selected browser's cookie database is locked, preventing misconfiguration.

## Phase 12: Bug Fixes & Enhancements
- [x] **Cancel downloads in queue**: Implemented functionality to cancel downloads that are in the queue but not actively running.
- [x] **Display download stats**: Added display for queued, active, and completed downloads in the app footer.
- [x] **yt-dlp Updater**: Removed update channel selection (always nightly) and added display of current `yt-dlp` version.
- [x] **Qt plugin deployment reliability (Debug/Release)**: Ensured CMake deploys both plugin/runtime variants so `QSQLITE` and TLS backends load correctly in Debug builds.
- [x] **Active download thumbnail plugin deployment**: Ensured CMake deploys the Qt `imageformats` plugins required for converted thumbnail previews (`qjpeg`, `qpng`, `qwebp`, `qico`) and improved widget-side thumbnail decode diagnostics.
- [x] **Stuck "Downloading..." with no transfer activity**: `YtDlpWorker` now resolves bundled `yt-dlp.exe` from app root or `bin/` and fails fast on process launch errors so items do not hang indefinitely.
- [x] **Window close lifecycle**: Clicking the main window close button (`X`) now exits the app instead of minimizing to tray/background.
- [x] **Unicode/special-character filename move reliability**: Download finalization now uses Qt-native move/copy fallback (not `cmd /c move`), while final filename resolution remains sourced from `yt-dlp --print after_move:filepath`.
- [x] **Canonical `settings.ini` cleanup**: `ConfigManager` now removes legacy sections like `[%General]`, strips deprecated duplicate aliases, and rewrites `settings.ini` in a canonical layout on save so each setting persists under one name.
- [ ] **Extractor domain list maintenance**: Periodically refresh `src/ui/assets/extractors.json` from yt-dlp so the app-directory `extractors.json` stays in sync.
