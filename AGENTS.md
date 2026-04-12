# AGENTS.md - AI Contributor Guide for MediaDownloader (C++ Port)

This document is the **canonical instruction set for all AI agents** working on the C++ port of the MediaDownloader project. It defines the project's purpose, architecture, constraints, and rules for safe and effective contributions.

All agents MUST follow this document as the source of truth.

**Fast Start:** Keep the UI responsive (no blocking I/O on the GUI thread; use `QThread` or `QtConcurrent`). Preserve the download lifecycle (temp → verify → move to completed dir). Prefer discovered or user-configured external binaries; bundled `bin/` copies are fallback-only while Phase 14 cleanup is in progress. Update `CHANGELOG.md` for significant changes. Update Section 3 + Quick-Reference list when files/roles change. For file locations, jump to the Quick-Reference list in Section 3.

---

## 1. Project Overview

**MediaDownloader (C++)** is a desktop application built with **Qt 6 (Widgets)** that allows users to download media (primarily video and audio) from online platforms using **yt-dlp** and **gallery-dl**.

**This project is a C++ port of the original Python application.** The primary goal is to create a **drop-in replacement** that is faster, more efficient, and maintains full compatibility with the user's existing settings and download history.

Key goals:
- Provide a **stable, user-friendly GUI** using native C++ and Qt.
- Support **concurrent downloads**, playlists, and advanced configuration.
- Be **self-contained** when compiled (no external dependency installs).
- Prioritize **UI responsiveness**, robustness, and clear error handling.
- **Maintain 100% compatibility** with `settings.ini` and `download_archive.db` from the Python version.

The target platform is primarily **Windows**, but path handling and logic should remain cross-platform where possible.

---

## 2. Core Functionality (What Must Not Break)

Agents MUST preserve and respect the following behaviors from the original Python application:

- Media downloading via `yt-dlp` (for videos/audio) and `gallery-dl` (for image galleries).
- Concurrent download management with user-defined limits.
- Playlist expansion and processing (for `yt-dlp` downloads).
- **Configuration Portability**: The C++ app MUST read and write to `settings.ini` in a format identical to the Python version.
- **Archive Portability**: The C++ app MUST use the same `download_archive.db` (SQLite) to respect the user's download history.
- File lifecycle: download into temp dir → verify file stability → move to completed downloads directory.
- Metadata embedding (title, artist, etc.) and thumbnail embedding.
- Responsive GUI at all times (no blocking I/O on the main thread).
- In-app updating of the `yt-dlp` and `gallery-dl` executables.
- The final executable name MUST be `MediaDownloader.exe` to ensure the update process from the Python version works seamlessly.
- **Download Progress Display**: The UI progress bar MUST update correctly for **both** yt-dlp's native downloader **and** aria2c as an external downloader. The worker must parse and emit progress from:
  - Native yt-dlp progress lines (`[download] XX.X% of ...`)
  - aria2c progress lines (`[#XXXXX N.NMiB/N.NMiB(XX.X%)...]`)
  - The `--progress-template` output format
- **Progress Bar Color Coding**: Download progress bars MUST use color-coding to provide visual feedback on download state:
  - **Colorless/Default** (no custom stylesheet): When download is queued, initializing, or in indeterminate state (progress < 0)
  - **Light Blue** (`#3b82f6`): While actively downloading (0% < progress < 100%)
  - **Teal** (`#008080`): During post-processing phase (progress at 100% with status containing "Processing", "Merging", or "Post")
  - **Green** (`#22c55e`): When download is fully completed (progress at 100% and post-processing finished)
- **Detailed Progress Display**: The UI MUST display rich, detailed progress information to users during downloads, comparable to command-line yt-dlp output:
  - **Status Label**: Shows current download stage (e.g., "Extracting media information...", "Downloading 2 segment(s)...", "Merging segments with ffmpeg...", "Verifying download completeness...", "Applying sorting rules...", "Moving to final destination...")
  - **Progress Details Label**: Below the progress bar, display formatted information including:
    - Downloaded size / Total size (e.g., "15.3 MiB / 45.7 MiB")
    - Download speed (e.g., "Speed: 2.4 MiB/s")
    - Estimated time remaining (e.g., "ETA: 0:12")
  - Information MUST be separated by bullet points (•) for readability
  - All progress data (speed, ETA, sizes) MUST be emitted by workers and parsed from both native yt-dlp and aria2c output

---

## 3. Architecture Overview (C++ Port)

**POLICY:** Agents MUST update this section and the "Quick-Reference" list below whenever new files are added or responsibilities shift.

The project follows a **modular, separation-of-concerns design** using C++ and Qt.

### Entry Point
- `main.cpp` - Initializes the `QApplication`, creates and shows the `MainWindow`, and enforces single application instance using `QSystemSemaphore` and `QSharedMemory`. Installs a custom message handler for logging.
- `CMakeLists.txt` - Project definition, dependencies (Qt6), and build instructions.
- `MediaDownloader.rc` - Windows resource file for embedding the application icon into the executable.

### UI Layer (`src/ui/`)
- `MainWindow.h/.cpp` - Application shell and signal orchestrator; initializes tabs, connects global signals. Now includes labels for displaying download statistics (queued, active, completed). **Handles initial setup prompt for download directories if not configured, ensuring both completed and temporary directories are set at launch.** **Connects to `AdvancedSettingsTab::setYtDlpVersion` to display the current `yt-dlp` version.** **Now includes a `QClipboard` listener and an updated `handleClipboardAutoPaste` function to support multiple auto-paste modes.** **Handles runtime subtitle selection and displays the runtime format-selection dialog requested by `DownloadManager`.**
- `StartTab.h/.cpp` - Input and configuration; URL field, download type selection, command preview, and "Add to Queue" logic. Now includes enhanced audio quality options, a broader list of audio codecs, and dynamic audio extension selection based on the chosen codec. Also includes new "Playlist" options (Ask, Download All, Download Single), "Max Concurrent" options (1-8, 1 (short sleep), 1 (long sleep)), and expanded "Rate Limit" options for finer control over download speeds. **Added "lock" checkboxes for Video Settings and Audio Settings to prevent accidental changes, with their states saved persistently.** **Now handles "Override duplicate download check" and "Enable SponsorBlock" settings.** **Must NOT expose ad-hoc runtime quality/codec dropdowns on the Start tab; runtime format selection belongs to Advanced Settings and download-time dialogs.**
- `ActiveDownloadsTab.h/.cpp` - Monitoring; renders active/completed downloads, progress bars, etc. Ensures a thumbnail preview is played/displayed on the left side of each download GUI element.
- `AdvancedSettingsTab.h/.cpp` - Global settings; UI for paths, concurrency, etc. **Refactored to use a `QListWidget` menu on the left and a `QStackedWidget` on the right to display settings in categories.** **Listens for `ConfigManager::settingChanged` signals to dynamically update displayed path values.** Settings are organized into logical groups: Configuration, Authentication Access, Output Template, Download Options, Metadata/Thumbnails, Subtitles, and Updates. Most settings auto-save on change. The "Output Template" has a dedicated "Save" button with validation. Subtitle language input is a `QComboBox` with full language names. The `yt-dlp` update channel selection has been removed (always nightly). Includes a `setYtDlpVersion` slot to update the displayed `yt-dlp` version. Provides immediate feedback if a selected browser's cookie database is locked, preventing misconfiguration. The cookie access check now has a 30-second timeout to prevent indefinite hangs. **The "Auto-paste URL when app is focused" toggle has been replaced with a `QComboBox` offering multiple auto-paste modes (disabled, on focus, on new URL, on focus & enqueue, on new URL & enqueue).**
    - The left-side category list now derives its colors from the active `QPalette` and re-applies the stylesheet when a palette change occurs so it stays compact and theme-accurate.
- `advanced_settings/BinariesPage.h/.cpp` - Advanced Settings page for external dependency management. **Shows per-binary status for discovered/configured executables, keeps manual "Browse" overrides, and offers package-manager/manual-download install actions for `yt-dlp`, `ffmpeg`, `ffprobe`, `gallery-dl`, `aria2c`, and `deno`.**
- `FormatSelectionDialog.h/.cpp` - Runtime format picker; displays `yt-dlp --dump-json` format data in a table, allows multi-selection/custom IDs, and each selected format is enqueued as a separate download. **This dialog is the sole place where runtime video/audio format decisions are made once Advanced Settings quality is set to `Select at Runtime`.**
- `RuntimeSelectionDialog.h/.cpp` - Runtime subtitle picker; currently used for subtitle-at-runtime selection sourced from extractor metadata.
- `SortingTab.h/.cpp` - UI for managing file sorting rules. **Now uses a `QTableWidget` to display rules in a grid with columns for Priority, Type, Condition, Target Path, and Subfolder.**
- `SortingRuleDialog.h/.cpp` - Dialog for creating and editing sorting rules. **Refactored to include a "Remove" button for each condition, improving usability, and now features a multi-line text input for values with a reduced minimum height of 60 pixels. Also includes "Greater Than" and "Less Than" operators, dynamically enabled/disabled based on the selected field. "Is One Of" condition values are now sorted alphabetically when the dialog is accepted.**
- `resources.qrc` - Qt Resource file for embedding assets like images.

### Core Logic (`src/core/`)
- `ConfigManager.h/.cpp` - State persistence; reads/writes `settings.ini` using `QSettings`. **Must be compatible with Python's `configparser` output.** Automatically sets `temporary_downloads_directory` when `completed_downloads_directory` is updated. **Emits `settingChanged` signal when any setting is modified.** **Now uses an internal map (`m_defaultSettings`) to manage default values, and includes `initializeDefaultSettings()` and `getDefault()` methods.** **The `yt_dlp_update_channel` setting has been removed.** **Ensures `output_template` is always a filename template and `temporary_downloads_directory` is correctly derived from `completed_downloads_directory`.** **On save, rewrites `settings.ini` into a canonical layout, removing legacy sections like `[%General]` and deprecated duplicate keys while ensuring all valid settings (including `SortingRules`) are preserved.** **The `General/auto_paste_on_focus` boolean setting has been replaced by `General/auto_paste_mode` integer setting to support multiple auto-paste options.** **Provides defaults for runtime-selection toggles such as `Video/video_multistreams` and `Audio/audio_multistreams`.**
- `ArchiveManager.h/.cpp` - History persistence; reads/writes `download_archive.db` using `QtSql`. **Must be compatible with Python's schema.**
- `DownloadManager.h/.cpp` - "Brain"; Manages the download queue, respects concurrency limits, and orchestrates workers. **Bypasses playlist expansion for "gallery" download types.** Now supports cancellation of downloads that are in the queue but not actively running. Emits `downloadStatsUpdated` signal with counts for queued, active, and completed downloads. **Uses `ConfigManager`'s `temporary_downloads_directory` to locate downloaded files for verification and moving. Normalizes file paths received from workers to prevent cross-platform issues.** **If Advanced Settings quality is set to `Select at Runtime` for video or audio downloads, it fetches `yt-dlp` format metadata asynchronously and asks `MainWindow` to present `FormatSelectionDialog`; each selected format is re-enqueued as its own download.** **Passes `ConfigManager` into `YtDlpWorker` so downloads can use configured or auto-detected executables instead of only bundled ones.**
- `SortingManager.h/.cpp` - "Helper"; Applies sorting rules to determine the final download directory.
- `PlaylistExpander.h/.cpp` - "Expander"; Uses `yt-dlp --flat-playlist --dump-single-json` to expand playlist URLs into individual video entries. **Now uses `YtDlpArgsBuilder` to construct the full command (including `--js-runtimes deno:...`, `--cookies-from-browser`, `--ffmpeg-location`, etc.) so playlist expansion matches the actual download configuration.**
- `Aria2Daemon.h/.cpp` - "Daemon"; Manages the background `aria2c` RPC server for concurrent, segmented downloads.
- `Aria2DownloadWorker.h/.cpp` - "Worker"; Orchestrates the pipeline (Extract -> Download -> Post-process) for media downloads.
- `YtDlpJsonExtractor.h/.cpp` - "Extractor"; Runs `yt-dlp --dump-json` asynchronously to fetch download metadata and URLs without blocking the UI.
- `FfmpegPostProcessor.h/.cpp` - "Muxer"; Asynchronously runs `ffmpeg` to merge audio, video, subtitles, and artwork safely.
- `GalleryDlWorker.h/.cpp` - "Muscle"; Wraps `QProcess` to run `gallery-dl`, parses stdout for progress, and emits signals. **Resolves `gallery-dl` through `ProcessUtils`, supporting user-configured/system binaries.**
- `GalleryDlArgsBuilder.h/.cpp` - "Helper"; Constructs command-line arguments for `gallery-dl` based on settings from `ConfigManager`. **Passes the full template directly to `-f` since gallery-dl natively supports path templates with `/` separators.**
- `YtDlpArgsBuilder.h/.cpp` - "Helper"; Constructs command-line arguments for `yt-dlp` based on settings from `ConfigManager`. **Now relies solely on `ConfigManager` for the `temporary_downloads_directory` and `output_template`.**
- `YtDlpWorker.h/.cpp` - yt-dlp process wrapper. **Resolves `yt-dlp` through `ProcessUtils`, supports user-configured/system binaries, and parses both native yt-dlp progress lines (`[download] XX.X% of ...`) and aria2 output while preserving UTF-8 metadata/title extraction.** Includes diagnostic logging for process state changes, stderr/stdout data received, and progress parsing.
- `AppUpdater.h/.cpp` - Self-maintenance; checks GitHub Releases for application updates.
- `StartupWorker.h/.cpp` - Orchestrates initial startup checks, including `yt-dlp` and `gallery-dl` version fetching and extractor list generation.

### Utilities (`src/utils/`)
- `StringUtils.h/.cpp` - Helper functions for string manipulation, URL normalization, etc. **The `cleanDisplayTitle` function has been removed as the video title is now extracted directly from `yt-dlp`'s `info.json` output.**
- `LogManager.h/.cpp` - Installs a custom message handler for structured logging, including log rotation.
- `BrowserUtils.h/.cpp` - Helper functions for browser-related tasks, such as finding installed browsers. The `checkCookieAccess` function has been removed.
- `ExtractorJsonParser.h/.cpp` - Loads the `extractors_yt-dlp.json` and `extractors_gallery-dl.json` databases from the app directory for clipboard URL auto-paste.

### Quick-Reference: Where is X?

- **Settings/Config**: `src/core/ConfigManager.h/.cpp` (handles `settings.ini` I/O, emits `settingChanged` signal, ensures `output_template` is a filename template, `temporary_downloads_directory` is correctly set, and rewrites `settings.ini` to remove legacy/duplicate keys such as `[%General]` and old `Video` aliases while ensuring all settings (including `SortingRules`) are preserved).
- **Download Archive**: `src/core/ArchiveManager.h/.cpp` (handles `download_archive.db` I/O).
- **URL Validation**: `src/core/DownloadManager.h/.cpp`.
- **Download Queue**: `src/core/DownloadManager.h/.cpp`.
- **Playlist Expansion**: `src/core/PlaylistExpander.h/.cpp` (uses `YtDlpArgsBuilder` for full command construction including deno, cookies, ffmpeg path).
- **Process Execution (`aria2c`)**: `src/Aria2Daemon.h/.cpp` and `src/Aria2DownloadWorker.h/.cpp`.
- **Process Execution (`yt-dlp`/`ffmpeg`)**: `src/YtDlpJsonExtractor.h/.cpp` and `src/FfmpegPostProcessor.h/.cpp` (asynchronous metadata and muxing).
- **Process Execution (`gallery-dl`)**: `src/core/GalleryDlWorker.h/.cpp` (wraps `gallery-dl` `QProcess`, resolves executable via `ProcessUtils` supporting system PATH and custom paths).
- **Output Templates**: `src/ui/advanced_settings/OutputTemplatesPage.h/.cpp` (UI for yt-dlp and gallery-dl filename templates). **gallery-dl template dropdown includes comprehensive token list.**
- **gallery-dl Output Template Handling**: `src/core/GalleryDlArgsBuilder.h/.cpp` (splits templates containing `/` into `output.directory` and filename parts for proper gallery-dl directory structure).
- **Smart URL Download Type Switching**: `src/ui/StartTab.h/.cpp` (auto-detects extractor support for pasted URLs and switches download type: yt-dlp-only → Video, gallery-dl-only → Gallery, both/neither → no change).
- **External Binary Status / Install Actions**: `src/ui/advanced_settings/BinariesPage.h/.cpp`.
- **yt-dlp Progress Parsing / Binary Resolution**: `src/core/YtDlpWorker.h/.cpp`. **Parses native yt-dlp stderr output for `[download] XX.X% of ...` lines directly (no `download:` prefix stripping). Also parses aria2c progress lines (`[#XXXXX ...]`). Caches full metadata from `info.json` in `m_fullMetadata` for sorting rule evaluation at download completion.**
- **Progress Bars**: `src/ui/ActiveDownloadsTab.h/.cpp` (updates UI based on worker signals).
- **App Updates**: `src/core/AppUpdater.h/.cpp`.
- **File Moving**: `src/core/DownloadManager.h/.cpp` (moves files from temp to final output, using `SortingManager` to determine the final directory).
- **Sorting Rules**: `src/ui/SortingTab.h/.cpp` (UI) and `src/core/SortingManager.h/.cpp` (logic).
- **Cookies from Browser (Video/Audio)**: `src/ui/AdvancedSettingsTab.h/.cpp` (handles cookie access checks directly using `QProcess`).
- **Cookies from Browser (Galleries)**: `src/ui/AdvancedSettingsTab.h/.cpp` (handles cookie access checks directly using `QProcess`).
- **Override duplicate download check**: `src/ui/StartTab.h/.cpp`.
- **Enable SponsorBlock**: `src/ui/StartTab.h/.cpp`.
- **Runtime video/audio format selection**: `src/core/DownloadManager.h/.cpp` (triggering/fetch), `src/ui/FormatSelectionDialog.h/.cpp` (selection UI), and `src/ui/MainWindow.h/.cpp` (dialog orchestration).
- **Runtime subtitle selection**: `src/ui/MainWindow.h/.cpp` and `src/ui/RuntimeSelectionDialog.h/.cpp`.
- **Auto-paste URL behavior**: `src/ui/AdvancedSettingsTab.h/.cpp` (setting via `QComboBox`) and `src/ui/MainWindow.h/.cpp` (clipboard monitoring and logic).
- **Advanced Settings navigation styling**: `src/ui/AdvancedSettingsTab.h/.cpp` (palette-driven `QListWidget` styling for the left menu, refreshed on palette changes so the tab stays compact and theme-accurate).
- **Embed video chapters**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Embed metadata**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Embed thumbnail**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Use high-quality thumbnail converter**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Convert thumbnails to**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Subtitle language**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Embed subtitles in video**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Write subtitles (separate file)**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Include automatically-generated subtitles**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Subtitle file format**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **yt-dlp version display**: `src/ui/AdvancedSettingsTab.h/.cpp` (display), `src/core/StartupWorker.h/.cpp` (initial fetch).
- **Update yt-dlp**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **gallery-dl version display**: `src/ui/AdvancedSettingsTab.h/.cpp` (display), `src/core/StartupWorker.h/.cpp` (initial fetch).
- **Update gallery-dl**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Binary auto-detection and fallback resolution**: `src/utils/BinaryFinder.h/.cpp` and `src/core/ProcessUtils.h/.cpp`. **ProcessUtils now caches resolved paths after the first lookup; `resolveBinary()` performs a fresh scan, `findBinary()` uses the cache. Cache is cleared when binaries are installed/overridden via BinariesPage.**
- **External Downloader (aria2c)**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **Restrict filenames**: `src/ui/AdvancedSettingsTab.h/.cpp`.
- **UI Assets**: `src/ui/resources.qrc` (for embedded images and other resources).
- **Executable Icon**: `MediaDownloader.rc` (Windows resource file for the application icon).
- **Extractor Domain Lists**: `extractors_yt-dlp.json` and `extractors_gallery-dl.json` (copied beside `MediaDownloader.exe` for URL auto-paste).
- **Extractor Loader**: `src/utils/ExtractorJsonParser.h/.cpp` (loads extractor domains from the app directory for clipboard checks).
- **Download Statistics Display**: `src/ui/MainWindow.h/.cpp` (labels for queued, active, completed counts).
- **Initial Directory Setup**: `src/ui/MainWindow.h/.cpp` (prompts user for download directories on first launch).
- **Logging**: `src/utils/LogManager.h/.cpp` (installs a custom message handler for structured logging with log rotation).
- **Download Item Widget**: `src/ui/DownloadItemWidget.h/.cpp` (displays thumbnail preview on the left side of the progress bar, loaded from `thumbnail_path` in progress data; updates title from progress data; maintains left-side thumbnail preview requirement).

---

## 4. Dependencies

The application is transitioning to an **unbundled external-binary model**.

Current expectations:
- Prefer user-installed or manually configured executables for `yt-dlp`, `ffmpeg`, `ffprobe`, `gallery-dl`, `aria2c`, and `deno`.
- Treat existing bundled copies in `bin/` or the app root as fallback compatibility paths only until Phase 14 cleanup removes them.
- Keep Qt runtime/plugin deployment self-contained, including `qsqlite.dll`.

Agents MUST NOT:
- Introduce new external runtime dependencies without explicit instruction.
- Break existing fallback support while bundled binaries still exist in the repository.
- Require blocking runtime downloads on the GUI thread.

---

## 5. Development Stack

- **Language**: C++20
- **Framework**: Qt 6 (Widgets, Core, Network, Sql)
- **Build System**: CMake
- **Database**: SQLite (via `QtSql` module)

---

## 6. Agent Rules (Read Carefully)

### You MUST:
- Keep the UI responsive (use `QThread`, `QtConcurrent`, or `QProcess`'s async API).
- **Update `CMakeLists.txt` for any new dependencies.** If you add code that requires a new Qt module (e.g., `QtXml`), library, or source file (`.cpp`, `.h`, `.ui`), you MUST update `CMakeLists.txt` accordingly.
    - **Example:** Adding a new class `MyClass` requires adding `src/core/MyClass.cpp` to the `add_executable` command in `CMakeLists.txt`.
    - **Example:** Using a new Qt module like `QtXml` requires adding `Xml` to the `find_package(Qt6 ... COMPONENTS ...)` list.
- **Preserve existing build paths and settings.** Do not modify existing `INCLUDEPATH` or `LIBS` entries in the build configuration unless it is the explicit goal of the task. The project relies on specific paths for its dependencies.
- **Assume a Windows (MSVC) primary toolchain.** While cross-platform compatibility is a goal, ensure all changes build correctly on Windows first. Avoid introducing Unix-specific flags or syntax.
- **Preserve the exact format and behavior** of `settings.ini` or the schema of `download_archive.db`. This is the top priority.
- Maintain clear, user-facing error messages.
- Respect the existing file lifecycle (temp -> final) and directory structure.
- Add logging (`QDebug`) for non-trivial changes.
- **Update AGENTS.md** (Architecture & Quick-Reference) if you add files or change core logic locations.
- **Ensure all UI elements have tooltips** (`setToolTip`).
- **Ensure Theme Compatibility**: All UI elements MUST be designed to work correctly in both light and dark themes. Avoid hardcoded colors; use the application's `QPalette` to ensure elements adapt to the current theme.
- **Update `SPEC.md`, `ARCHITECTURE.MD`, and `TODO.md`** to reflect any changes to functional requirements, system design, or pending tasks.
- **Discard Invalid Settings**: If any setting loaded from `settings.ini` does not match the current application's expected format, it MUST be discarded and replaced with the default value. The application MUST NOT attempt to migrate or interpret legacy formats.
- **Update Documentation on Functional Changes**: When you make changes to how the app works (e.g., progress parsing, download pipeline, UI behavior, configuration, external binary handling), you MUST update the relevant MD documentation files (`AGENTS.md`, `SPEC.md`, `ARCHITECTURE.md`, `TODO.md`, `CHANGELOG.md`) to reflect the new behavior. This is a mandatory requirement - do not leave documentation out of sync with the code.

### You MUST NOT:
- Change the format of `settings.ini` or the schema of `download_archive.db`.
- Introduce new external runtime dependencies without explicit instruction.
- Break the standalone, portable nature of the application.
- Assume network availability beyond what's needed for `yt-dlp` and the app updater.

---

## 7. Task Tracking

Agents MUST use `TODO.md` to track pending tasks, planned features, and known issues. Before starting work, check `TODO.md` for high-priority items. After completing a task or identifying a new one, update `TODO.md` accordingly.
