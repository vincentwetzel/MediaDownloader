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
- [x] **Installer**: Create and test the NSIS installer.
- [x] **UI Enhancements**:
    - [x] Dynamically populate video quality and codec options from `yt-dlp` output.
    - [x] Expanded "Rate Limit" dropdown options to include a wider range of slower speeds.
    - [x] **Advanced Settings Tab Reorganization**: Implemented new organization, auto-saving for most settings, dedicated save button for output template with validation, subtitle language combo box with full names, and removal of `yt-dlp` update channel selection.
    - [x] **Advanced Settings Navigation Styling**: Left-side category list derives its colors from the active palette and re-applies on palette changes so the layout is compact and theme-consistent.
    - [x] **Start Tab Settings Relocation**: Moved "Override duplicate download check" and "Enable SponsorBlock" to StartTab.
    - [x] **Start Tab URL Input Theming**: The URL `QTextEdit` now rebuilds its palette-based stylesheet when the palette changes so borders/background match light/dark modes.
    - [x] Provides immediate feedback if a selected browser's cookie database is locked, preventing misconfiguration.

## Phase 12: Bug Fixes & Enhancements
- [x] **Restore Defaults Safety**: Prevent "Restore Defaults" from wiping out dynamic `SortingRules`, UI `Theme`, output templates, and binary paths.
- [x] **Cancel downloads in queue**: Implemented functionality to cancel downloads that are in the queue but not actively running.
- [x] **Display download stats**: Added display for queued, active, and completed downloads in the app footer.
- [x] **yt-dlp Updater**: Removed update channel selection (always nightly) and added display of current `yt-dlp` version.
- [x] **Qt plugin deployment reliability (Debug/Release)**: Ensured CMake deploys both plugin/runtime variants so `QSQLITE` and TLS backends load correctly in Debug builds.
- [x] **Active download thumbnail plugin deployment**: Ensured CMake deploys the Qt `imageformats` plugins required for converted thumbnail previews (`qjpeg`, `qpng`, `qwebp`, `qico`) and improved widget-side thumbnail decode diagnostics.
- [x] **Stuck "Downloading..." with no transfer activity**: `YtDlpWorker` now resolves bundled `yt-dlp.exe` from app root or `bin/` and fails fast on process launch errors so items do not hang indefinitely.
- [x] **Window close lifecycle**: Clicking the main window close button (`X`) now exits the app instead of minimizing to tray/background.
- [x] **Unicode/special-character filename move reliability**: Download finalization now uses Qt-native move/copy fallback (not `cmd /c move`), while final filename resolution remains sourced from `yt-dlp --print after_move:filepath`.
- [x] **Canonical `settings.ini` cleanup**: `ConfigManager` now removes legacy sections like `[%General]`, strips deprecated duplicate aliases, and rewrites `settings.ini` in a canonical layout on save so each setting persists under one name.
- [x] **Extractor domain list maintenance**: Adopted existing `update_yt-dlp_extractors.py` and `update_gallery-dl_extractors.py` scripts as developer-side tooling to ensure the generated JSON files retain their critical domain-mapping structures.
- [x] **Thumbnail Previews**: Restored the ability to play/display thumbnail previews for audio/video downloads via `QDesktopServices::openUrl`.

## Phase 13: Future Enhancements
- [x] **Ensure Downloads Have a Cancel/Retry Button**: (High Priority)
    - [x] Verify that all active downloads display a cancel button.
    - [x] If a download is cancelled by the user or if it fails, the cancel button should be replaced by a retry button to allow the user to restart it.
- [x] **Interface Languages**: Add support for multiple interface languages including English, Mandarin, Spanish, Hindi, Portuguese, Bengali, Russian, Japanese, Western Punjabi, Turkish, Vietnamese, Yue Chinese, Egyptian Arabic, Wu Chinese, Marathi, Telugu, Korean, Tamil, Urdu, Indonesian, German, French, Javanese, Iranian Persian, Italian, Hausa, Gujarati, Levantine Arabic, and Bhojpuri.
    - [x] Add a dropdown at the top right of the app, resting in the corner of the Tab Widget.
    - [x] Keep English and other languages common in the U.S. and Europe pinned to the top of the list.
    - [x] Display country flags next to each language in the dropdown to visually assist users in finding their language.
- [x] **Runtime Format/Stream Selection**: Allow users to defer quality/codec/stream selection until runtime.
    - [x] Use `Quality = Select at Runtime` in Video and Audio Advanced Settings as the mode switch for runtime format picking.
    - [x] Hide the other format defaults on the active settings page while runtime mode is selected so the workflow stays all-defaults or all-runtime.
    - [x] Intercept downloads in `DownloadManager` to check for this setting. Do not add runtime quality/codec dropdowns to `StartTab`.
    - [x] Asynchronously fetch available formats using `yt-dlp -J` or `YtDlpJsonExtractor`.
    - [x] Present a `FormatSelectionDialog` allowing the user to select their desired video/audio format IDs.
    - [x] Enqueue one download per selected runtime format.
- [x] **Backup Download Queue**: Persistently store the download queue to disk.
    - [x] If the application crashes or exits before all downloads complete, the app should load this file on the next launch.
    - [x] Prompt the user with a dialogue box asking which incomplete/queued downloads they would like to resume or discard.
- [x] **Clear Completed Downloads**:
    - [x] Add a "Clear completed downloads" button at the top of the Active Downloads tab to remove all successfully completed downloads from the GUI.
    - [x] Add a small 'X' button on the top right of each individual download GUI after it fully completes to clear it from the list.
    - [x] **Auto-clear option**: Add a setting to automatically clear completed downloads from the Active Downloads tab without manual intervention.
- [x] **Cancel All Downloads**:
    - [x] Add a GUI button on the Active Downloads tab to cancel ALL downloads.
    - [x] This should immediately cancel any currently running downloads and completely clear the remaining download queue.
- [x] **Pause/Resume Downloads**:
    - [x] Add a "Pause All Downloads" button to the Active Downloads tab. When all are paused, it should change to "Resume All Downloads".
    - [x] Allow users to pause/resume individual downloads.
- [x] **Change Download Queue Ordering**:
    - [x] Visually order downloads in the Active Downloads tab to match their underlying queue order.
    - [x] Add up/down buttons on each download to shift it up or down in the GUI and the download queue.
    - [x] Ensure that shifting a currently running download down does not stop it; this feature is only for determining the order of queued downloads.
- [x] **Long Term: Unbundled Binaries Support**:
    - [x] Modify the application so it can run without having binaries bundled.
    - [x] The app should first look for required binaries (like `yt-dlp`, `ffmpeg`, etc.) in the system PATH.
    - [x] Backend logic now uses custom paths for `yt-dlp`, `ffmpeg`, `ffprobe`, `gallery-dl`, and `aria2c` via `ProcessUtils`.
    - [x] Add a dedicated page in Advanced Settings to manage binary paths.
    - [x] If binaries are found in the PATH, their location should be listed in the settings.
    - [x] Provide a "Browse" button for users to manually locate already installed binaries.
    - [x] If binaries are missing, the app should provide detailed dialogue boxes guiding the user through downloading and installing them.
    - [x] The app must still support updating these binaries during startup.
    - [x] Handle edge cases, such as updating `yt-dlp` using `pip upgrade` if it was installed via PIP, while not strictly requiring the user to have Python installed.
- [x] **Generate folder.jpg for Audio Playlists**:
    - [x] Add an option specifically for audio playlist downloads to generate a `folder.jpg` file in the playlist directory.
    - [x] This must be a separate option from embedding thumbnails into individual audio files.
- [x] **Subtitle Selection Improvements**:
    - [x] Change the current subtitle language selection from a single dropdown to a dialogue box with checkboxes to allow multi-selection.
    - [x] Ensure that missing subtitles in the requested languages do not break the rest of the download process.
    - [x] Add a "Select at Runtime" option for subtitles. When selected, prompt the user via a dialogue box at download time to pick from a list of actually available subtitles for that media item.
- [x] **Settings Storage Location**:
    - [x] In the final release build, change the default storage location of `settings.ini` from the application directory to the system's standard user data location (e.g., `%APPDATA%` on Windows, `~/.config` on Linux, `~/Library/Application Support` on macOS).
- [x] **Crop Audio Thumbnails to Square**:
    - [x] Add a toggle switch setting (enabled by default) for audio downloads to have their thumbnails cropped as a square.
    - [x] Implementation requires adding `ThumbnailsConvertor+ffmpeg_o:-q:v 0 -vf crop=ih` to the `yt-dlp` command.
- [x] **Split Output Templates (Video vs Audio)**:
    - [x] Split the output template settings in the UI so users can have different templates for video downloads and audio downloads.
    - [x] Backend logic to pull `output_template_video` vs `output_template_audio` based on download type.
- [x] **Expand Output Template Fields**:
    - [x] Fully implement all available fields for the `yt-dlp` output template.
    - [x] Keep current favorite options at the top of the dropdown, but add the rest of the possible `yt-dlp` fields to the list.
- [x] **Implement `--split-chapters` Support**:
    - [x] UI checkbox for splitting chapters.
    - [x] Backend logic to inject `--split-chapters` option.
- [x] **Geo-Verification Proxy**:
    - [x] Add an option in Advanced Settings UI for `yt-dlp` downloads to use `--geo-verification-proxy`.
    - [x] Backend logic to inject `--geo-verification-proxy` from config.
