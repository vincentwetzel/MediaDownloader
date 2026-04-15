# AGENTS.md - AI Contributor Guide for LzyDownloader (C++ Port)

This document is the **canonical instruction set for all AI agents** working on the C++ port of the LzyDownloader project. It defines the project's purpose, architecture, constraints, and rules for safe and effective contributions.

All agents MUST follow this document as the source of truth.

**Fast Start:** Keep the UI responsive (no blocking I/O on the GUI thread; use `QThread` or `QtConcurrent`). Preserve the download lifecycle (temp → verify → move to completed dir). Prefer discovered or user-configured external binaries. Update `CHANGELOG.md` for significant changes. Update Section 3 + Quick-Reference list when files/roles change. For file locations, jump to the Quick-Reference list in Section 3.

---

## 1. Project Overview

**LzyDownloader (C++)** is a desktop application built with **Qt 6 (Widgets)** that allows users to download media (primarily video and audio) from online platforms using **yt-dlp** and **gallery-dl**.

**This project is a C++ port of the original Python application.** The primary goal is to create a **drop-in replacement** that is faster, more efficient, and maintains full compatibility with the user's existing settings and download history.

Key goals:
- Provide a **stable, user-friendly GUI** using native C++ and Qt.
- Support **concurrent downloads**, playlists, and advanced configuration.
- Be **self-contained** when compiled (no external dependency installs).
- Prioritize **UI responsiveness**, robustness, and clear error handling.
- **Clean Break from Python**: Backwards compatibility with the Python version's `settings.ini` is NO LONGER REQUIRED. It is expected and acceptable that the C++ app uses a pure Qt-native configuration format, even if it requires users to regenerate their settings.

The target platform is primarily **Windows**, but path handling and logic should remain cross-platform where possible.

---

## 2. Core Functionality (What Must Not Break)

Agents MUST preserve and respect the following behaviors from the original Python application:

- Media downloading via `yt-dlp` (for videos/audio) and `gallery-dl` (for image galleries).
- Concurrent download management with user-defined limits.
- Playlist expansion and processing (for `yt-dlp` downloads).
- **Configuration**: The app uses a Qt-native `QSettings` INI implementation. It does not need to conform to Python `configparser` quirks.
- **Archive Portability**: The C++ app MUST use the same `download_archive.db` (SQLite) to respect the user's download history.
- File lifecycle: download into temp dir → verify file stability → move to completed downloads directory.
- Metadata embedding (title, artist, etc.) and thumbnail embedding.
- Responsive GUI at all times (no blocking I/O on the main thread).
- In-app updating of the `yt-dlp` and `gallery-dl` executables.
- The final executable name MUST be `LzyDownloader.exe` to ensure the update process from the Python version works seamlessly.
- **Download Progress Display**: The UI progress bar MUST update correctly for **both** yt-dlp's native downloader **and** aria2c as an external downloader. The worker must parse and emit progress from:
  - Native yt-dlp progress lines (`[download] XX.X% of ...`)
  - aria2c progress lines (`[#XXXXX N.NMiB/N.NMiB(XX.X%)...]`)
  - The `--progress-template` output format
  - Livestream indeterminate progress (`[download] XX.XMiB at YY.YMiB/s (HH:MM:SS)`)
- **Progress Bar Color Coding**: Download progress bars MUST use color-coding to provide visual feedback on download state:
  - **Colorless/Default** (no custom stylesheet): When download is queued, initializing, or in indeterminate state (progress < 0)
  - **Light Blue** (`#3b82f6`): While actively downloading (0% < progress < 100%)
  - **Teal** (`#008080`): During post-processing phase (progress at 100% with status containing "Processing", "Merging", or "Post")
  - **Green** (`#22c55e`): When download is fully completed (progress at 100% and post-processing finished)
  - The percentage, file sizes, speed, and ETA MUST be painted centered on the progress bar using the `ProgressLabelBar` custom widget.
- **Detailed Progress Display**: The UI MUST display rich, detailed progress information to users during downloads, comparable to command-line yt-dlp output:
  - **Status Label**: Shows current download stage (e.g., "Extracting media information...", "Downloading 2 segment(s)...", "Merging segments with ffmpeg...", "Verifying download completeness...", "Applying sorting rules...", "Moving to final destination...", "Next check in 05:00")
  - **Centered Progress Text**: Painted directly on the progress bar, includes percentage, downloaded/total size, speed, and ETA (e.g., "45%  15.3 MiB/45.7 MiB  2.4 MiB/s  ETA 0:12")
  - All progress data (speed, ETA, sizes) MUST be emitted by workers and parsed from both native yt-dlp and aria2c output
- **Immediate Queue UI Feedback**: Downloads MUST appear in the Active Downloads tab immediately when queued, without waiting for playlist expansion or validation:
  - Gallery downloads appear instantly with "Queued" status
  - Video/audio downloads appear instantly with "Checking for playlist..." status during playlist expansion
  - Single videos update to "Queued" once expansion completes
  - Playlists remove the placeholder and add individual items for each track
  - Queue state persistence (`saveQueueState`) MUST be deferred via `Qt::QueuedConnection` to avoid blocking the GUI thread

---

## 3. Architecture Overview (C++ Port)

**POLICY:** Agents MUST update this section and the "Quick-Reference" list below whenever new files are added or responsibilities shift.

The project follows a **modular, separation-of-concerns design** using C++ and Qt.

### Entry Point
- `main.cpp` - Initializes the `QApplication`, creates and shows the `MainWindow`, and enforces single application instance using `QSystemSemaphore` and `QSharedMemory`. Installs a custom message handler for logging.
- `CMakeLists.txt` - Project definition, **version source of truth** (`project(VERSION x.y.z)`), dependencies (Qt6), and build instructions. Version is auto-generated into `version.h` via `configure_file`.
- `LzyDownloader.rc` - Windows resource file for embedding the application icon and **version info** (file/product version from `version.h`) into the executable.
- `src/core/version.h.in` - CMake template that generates `version.h` with `APP_VERSION_MAJOR`, `APP_VERSION_MINOR`, `APP_VERSION_PATCH`, `APP_VERSION_STRING`, and `APP_VERSION_RC` macros.
- **Auto version bump**: `.git/hooks/pre-push` automatically increments the patch version on every `git push`. Skip with `SKIP_VERSION_BUMP=1 git push`.

### UI Layer (`src/ui/`)
- `MainWindow.h/.cpp` - Application shell and signal orchestrator; initializes tabs, connects global signals. Now includes labels for displaying download statistics (queued, active, completed). **Handles initial setup prompt for download directories if not configured, ensuring both completed and temporary directories are set at launch.** **Connects to `AdvancedSettingsTab::setYtDlpVersion` to display the current `yt-dlp` version.** **Now includes a `QClipboard` listener and an updated `handleClipboardAutoPaste` function to support multiple auto-paste modes.** **Handles runtime subtitle selection and displays the runtime format-selection dialog requested by `DownloadManager`.**
- `StartTab.h/.cpp` - Input and configuration orchestrator. Delegates URL handling, download actions, and command preview updates to specialized helper classes.
- `StartTabUiBuilder.h/.cpp` - Builds the UI layout for `StartTab`, including URL input, download buttons, and quick-access folder buttons.
- `StartTabDownloadActions.h/.cpp` - Handles download button clicks, format checking, and download type changes.
- `StartTabUrlHandler.h/.cpp` - Manages URL text input, clipboard monitoring, and auto-switching download types based on supported extractors.
- `StartTabCommandPreviewUpdater.h/.cpp` - Updates the command preview text box when settings or download types change.
- `ActiveDownloadsTab.h/.cpp` - Monitoring; renders active/completed downloads, progress bars, etc. Ensures a thumbnail preview is played/displayed on the left side of each download GUI element. Includes toolbar buttons to quickly open temporary and completed download folders.
- `AdvancedSettingsTab.h/.cpp` - Global settings; UI for paths, concurrency, etc. **Refactored to use a `QListWidget` menu on the left and a `QStackedWidget` on the right to display settings in categories.** **Listens for `ConfigManager::settingChanged` signals to dynamically update displayed path values.** Settings are organized into logical groups: Configuration, Authentication Access, Output Template, Download Options, Metadata/Thumbnails, Subtitles, and Updates. Most settings auto-save on change. The "Output Template" has a dedicated "Save" button with validation. Subtitle language input is a `QComboBox` with full language names. The `yt-dlp` update channel selection has been removed (always nightly). Includes a `setYtDlpVersion` slot to update the displayed `yt-dlp` version. Provides immediate feedback if a selected browser's cookie database is locked, preventing misconfiguration. The cookie access check now has a 30-second timeout to prevent indefinite hangs. **The "Auto-paste URL when app is focused" toggle has been replaced with a `QComboBox` offering multiple auto-paste modes (disabled, on focus, on new URL, on focus & enqueue, on new URL & enqueue). All auto-paste modes now include duplicate URL prevention (5-second cooldown + queue checking).**
    - The left-side category list now derives its colors from the active `QPalette` and re-applies the stylesheet when a palette change occurs so it stays compact and theme-accurate.
- `advanced_settings/BinariesPage.h/.cpp` - Advanced Settings page for external dependency management. **Shows per-binary status for discovered/configured executables, keeps manual "Browse" overrides, and offers package-manager/manual-download install actions for `yt-dlp`, `ffmpeg`, `ffprobe`, `gallery-dl`, `aria2c`, and `deno`.**
- `FormatSelectionDialog.h/.cpp` - Runtime format picker; displays `yt-dlp --dump-json` format data in a table, allows multi-selection/custom IDs, and each selected format is enqueued as a separate download. Automatically filters out video formats when the 'Audio Only' download type is selected. **This dialog is the sole place where runtime video/audio format decisions are made once Advanced Settings quality is set to `Select at Runtime`.**
- `RuntimeSelectionDialog.h/.cpp` - Runtime subtitle picker; currently used for subtitle-at-runtime selection sourced from extractor metadata.
- `SortingTab.h/.cpp` - UI for managing file sorting rules. **Now uses a `QTableWidget` to display rules in a grid with columns for Priority, Type, Condition, Target Path, and Subfolder.**
- `SortingRuleDialog.h/.cpp` - Dialog for creating and editing sorting rules. **Uses QScrollArea with QVBoxLayout instead of QListWidget for smooth pixel-level scrolling (no item-snapping). Conditions are added directly to a vertical layout inside the scroll area. The dialog has a minimum size of 650x500. Multi-line text inputs use a fixed height of 100px controlled by the `CONDITION_VALUE_INPUT_HEIGHT` constant for consistent sizing. Also includes "Greater Than" and "Less Than" operators, dynamically enabled/disabled based on the selected field. "Is One Of" condition values are now sorted alphabetically when the dialog is accepted.**
- `resources.qrc` - Qt Resource file for embedding assets like images.

### Core Logic (`src/core/`)
- `ConfigManager.h/.cpp` - State persistence; reads/writes `settings.ini` using `QSettings`. Automatically sets `temporary_downloads_directory` when `completed_downloads_directory` is updated. Emits `settingChanged` signal when any setting is modified. Uses an internal map (`m_defaultSettings`) to manage default values. Ensures `output_template` is always a filename template. Automatically prunes dead/legacy keys from the configuration file on startup.
- `ArchiveManager.h/.cpp` - History persistence; reads/writes `download_archive.db` using `QtSql`. **Must be compatible with Python's schema.**
- `DownloadManager.h/.cpp` - "Brain"; Manages the download queue, respects concurrency limits, and orchestrates workers. **Bypasses playlist expansion for "gallery" download types.** Now supports cancellation of downloads that are in the queue but not actively running. Emits `downloadStatsUpdated` signal with counts for queued, active, and completed downloads. **Emits signals for UI prompts (playlist selection, queue resuming) rather than blocking the thread.** **If Advanced Settings quality is set to `Select at Runtime` for video or audio downloads, it fetches `yt-dlp` format metadata asynchronously and asks `MainWindow` to present `FormatSelectionDialog`; each selected format is re-enqueued as its own download.** **Passes `ConfigManager` into `YtDlpWorker` so downloads can use configured or auto-detected executables instead of only bundled ones.**
- `SortingManager.h/.cpp` - "Helper"; Applies sorting rules to determine the final download directory.
- `PlaylistExpander.h/.cpp` - "Expander"; Uses `yt-dlp --flat-playlist --dump-single-json` to expand playlist URLs into individual video entries. **Now uses `YtDlpArgsBuilder` to construct the full command (including `--js-runtimes deno:...`, `--cookies-from-browser`, `--ffmpeg-location`, etc.) so playlist expansion matches the actual download configuration.**
- `DownloadQueueManager.h/.cpp` - "Queue Master"; Manages the download queue, paused items, and pending playlist expansions. Handles saving/loading queue state and duplicate detection across all queue states.
- `DownloadFinalizer.h/.cpp` - "Mover"; Handles file stability verification, applying sorting rules, and moving/copying files from the temporary directory to their final destinations. Emits events back to the manager upon success or failure.
- `DownloadItem.h` - "Data Model"; Lightweight struct representing a single download item's state, options, and metadata.
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

- **Settings/Config**: `src/core/ConfigManager.h/.cpp` (handles `settings.ini` I/O, emits `settingChanged` signal, ensures `output_template` is a filename template, `temporary_downloads_directory` is correctly set, automatically prunes legacy keys on startup).
- **Download Archive**: `src/core/ArchiveManager.h/.cpp` (handles `download_archive.db` I/O).
- **URL Validation**: `src/core/DownloadManager.h/.cpp`.
- **Download Queue**: `src/core/DownloadQueueManager.h/.cpp`. **Manages the download queue, paused items, and pending playlist expansions. Provides immediate UI feedback by emitting download items before playlist expansion completes. Single videos show "Checking for playlist..." status which updates to "Queued" once expansion finishes. Gallery downloads appear instantly. Queue state persistence is handled internally, deferred via `Qt::QueuedConnection` to avoid blocking the GUI thread. Includes `getDuplicateStatus()` method that checks all states (queued, active, paused, completed) and emits `duplicateDownloadDetected` signal with user-friendly warning when duplicates are detected.**
- **Playlist Expansion**: `src/core/PlaylistExpander.h/.cpp` (uses `YtDlpArgsBuilder` for full command construction including deno, cookies, ffmpeg path).
- **Process Execution (`aria2c`)**: `src/Aria2Daemon.h/.cpp` and `src/Aria2DownloadWorker.h/.cpp`.
- **Process Execution (`yt-dlp`/`ffmpeg`)**: `src/YtDlpJsonExtractor.h/.cpp` and `src/FfmpegPostProcessor.h/.cpp` (asynchronous metadata and muxing).
- **Process Execution (`gallery-dl`)**: `src/core/GalleryDlWorker.h/.cpp` (wraps `gallery-dl` `QProcess`, resolves executable via `ProcessUtils` supporting system PATH and custom paths).
- **Output Templates**: `src/ui/advanced_settings/OutputTemplatesPage.h/.cpp` (UI for yt-dlp and gallery-dl filename templates). **gallery-dl template dropdown includes comprehensive token list.**
- **gallery-dl Output Template Handling**: `src/core/GalleryDlArgsBuilder.h/.cpp` (splits templates containing `/` into `output.directory` and filename parts for proper gallery-dl directory structure).
- **Smart URL Download Type Switching**: `src/ui/StartTab.h/.cpp` (auto-detects extractor support for pasted URLs and switches download type: yt-dlp-only → Video, gallery-dl-only → Gallery, both/neither → no change).
- **External Binary Status / Install Actions**: `src/ui/advanced_settings/BinariesPage.h/.cpp`.
- **yt-dlp Progress Parsing / Binary Resolution**: `src/core/YtDlpWorker.h/.cpp`. **Parses native yt-dlp stderr output for `[download] XX.X% of ...` lines directly (no `download:` prefix stripping). Also parses aria2c progress lines (`[#XXXXX ...]`). Handles `[wait]` states by parsing countdown timers and fetching pre-wait metadata (using YouTube oEmbed API for instant YouTube fetches, or a background `yt-dlp` process for others) to display title and thumbnail during long waits. Caches full metadata from `info.json` in `m_fullMetadata` for sorting rule evaluation at download completion.**
- **Progress Bars**: `src/ui/ActiveDownloadsTab.h/.cpp` (updates UI based on worker signals).
- **App Updates**: `src/core/AppUpdater.h/.cpp`.
- **File Moving**: `src/core/DownloadFinalizer.h/.cpp` (moves files from temp to final output, using `SortingManager` to determine the final directory).
- **Sorting Rules**: `src/ui/SortingTab.h/.cpp` (UI) and `src/core/SortingManager.h/.cpp` (logic).
- **Cookies from Browser (Video/Audio)**: `src/ui/AdvancedSettingsTab.h/.cpp` (handles cookie access checks directly using `QProcess`).
- **Cookies from Browser (Galleries)**: `src/ui/AdvancedSettingsTab.h/.cpp` (handles cookie access checks directly using `QProcess`).
- **Override duplicate download check**: `src/ui/StartTabDownloadActions.h/.cpp` (for saving config) and `src/ui/StartTabCommandPreviewUpdater.h/.cpp` (for command preview).
- **Enable SponsorBlock**: `src/ui/StartTabDownloadActions.h/.cpp` (for saving config) and `src/ui/StartTabCommandPreviewUpdater.h/.cpp` (for command preview).
- **Runtime video/audio format selection**: `src/core/DownloadManager.h/.cpp` (triggering/fetch), `src/ui/FormatSelectionDialog.h/.cpp` (selection UI), and `src/ui/MainWindow.h/.cpp` (dialog orchestration).
- **Runtime subtitle selection**: `src/ui/MainWindow.h/.cpp` and `src/ui/RuntimeSelectionDialog.h/.cpp`.
- **Auto-paste URL behavior**: `src/ui/AdvancedSettingsTab.h/.cpp` (setting via `QComboBox`) and `src/ui/MainWindow.h/.cpp` (clipboard monitoring and logic). **Includes 5-second cooldown and queue-duplicate checking to prevent multiple enqueues of the same URL.**
- **Duplicate download prevention**: `src/core/DownloadManager.h/.cpp` (`getDuplicateStatus()` checks queued, active, paused, and completed states; emits `duplicateDownloadDetected` signal), `src/ui/StartTab.h/.cpp` (displays warning popup with reason and URL).
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
- **Executable Icon**: `LzyDownloader.rc` (Windows resource file for the application icon).
- **Extractor Domain Lists**: `extractors_yt-dlp.json` and `extractors_gallery-dl.json` (copied beside `LzyDownloader.exe` for URL auto-paste).
- **Extractor Loader**: `src/utils/ExtractorJsonParser.h/.cpp` (loads extractor domains from the app directory for clipboard checks).
- **Download Statistics Display**: `src/ui/MainWindow.h/.cpp` (labels for queued, active, completed counts).
- **Initial Directory Setup**: `src/ui/MainWindow.h/.cpp` (prompts user for download directories on first launch).
- **Logging**: `src/utils/LogManager.h/.cpp` (installs a custom message handler for structured logging. **Creates one log file per run with timestamp in filename: `LzyDownloader_YYYY-MM-dd_HH-mm-ss.log`. Keeps up to 10 most recent log files, deleting older ones automatically**).
- **Download Item Widget**: `src/ui/DownloadItemWidget.h/.cpp` (displays thumbnail preview on the left side of the progress bar, loaded from `thumbnail_path` in progress data; updates title from progress data; **uses custom `ProgressLabelBar` subclass that paints percentage, sizes, speed, and ETA centered directly on the progress bar**).

---

## 4.9 UI Builders (`src/ui/MainWindowUiBuilder.h/.cpp`, `src/ui/StartTabUiBuilder.h/.cpp`)
- **Responsibilities:** Encapsulate the creation and layout of UI elements for `MainWindow` and `StartTab` respectively. They provide methods to build the UI and return pointers to the created widgets, allowing the main classes to manage logic and connections without being cluttered with UI construction details.

## 4. Dependencies

The application is transitioning to an **unbundled external-binary model**.

Current expectations:
- Prefer user-installed or manually configured executables for `yt-dlp`, `ffmpeg`, `ffprobe`, `gallery-dl`, `aria2c`, and `deno`.
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
- **Preserve the schema** of `download_archive.db`. (Note: `settings.ini` format is now pure Qt, backwards compatibility with Python is not required).
- Maintain clear, user-facing error messages.
- Respect the existing file lifecycle (temp -> final) and directory structure.
- Add logging (`QDebug`) for non-trivial changes.
- **Update AGENTS.md** (Architecture & Quick-Reference) if you add files or change core logic locations.
- **Ensure all UI elements have tooltips** (`setToolTip`).
- **File Size Limits (Context Preservation)**: Ensure that no single file (source code, headers, or documentation) exceeds **500 lines** in length (and `.md` files remain under 100KB) to preserve agent context usage. Refactor large C++ classes or split extensive markdown documents into smaller, logically separated files when approaching this limit.
- **Ensure Theme Compatibility**: All UI elements MUST be designed to work correctly in both light and dark themes. Avoid hardcoded colors; use the application's `QPalette` to ensure elements adapt to the current theme.
- **Update `SPEC.md`, `ARCHITECTURE.MD`, and `TODO.md`** to reflect any changes to functional requirements, system design, or pending tasks.
- **Discard Invalid Settings**: If any setting loaded from `settings.ini` does not match the current application's expected format, it MUST be discarded and replaced with the default value.
- **Update Documentation on Functional Changes**: When you make changes to how the app works (e.g., progress parsing, download pipeline, UI behavior, configuration, external binary handling), you MUST update the relevant MD documentation files (`AGENTS.md`, `SPEC.md`, `ARCHITECTURE.md`, `TODO.md`, `CHANGELOG.md`) to reflect the new behavior. This is a mandatory requirement - do not leave documentation out of sync with the code.
- **Use Q_INVOKABLE for Deferred Calls**: Methods called via `QMetaObject::invokeMethod` with `Qt::QueuedConnection` MUST be declared as `Q_INVOKABLE` in the header file, even if they are in the `private` or `private slots` sections. Without this, the invocation will fail silently at runtime with a warning like `No such method DownloadManager::saveQueueState()`.

### You MUST NOT:
- Change the schema of `download_archive.db` without a migration plan.
- Introduce new external runtime dependencies without explicit instruction.
- Break the standalone, portable nature of the application.
- Assume network availability beyond what's needed for `yt-dlp` and the app updater.

---

## 7. Task Tracking

Agents MUST use `TODO.md` to track pending tasks, planned features, and known issues. Before starting work, check `TODO.md` for high-priority items. After completing a task or identifying a new one, update `TODO.md` accordingly.
