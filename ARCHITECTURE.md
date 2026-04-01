# MediaDownloader C++ Architecture

## 1. Overview
This document outlines the architecture for the C++ port of MediaDownloader. The application is built with **Qt 6 (Widgets)** and provides a graphical interface for downloading media using **yt-dlp** and **gallery-dl**. The design prioritizes performance, stability, and seamless compatibility with the original Python version.

## 2. System Design

### 2.1 Single Instance Enforcement
The application ensures that only one instance can run at a time. This is achieved in `main.cpp` using `QSystemSemaphore` and `QSharedMemory`. A `QSystemSemaphore` with a unique key is acquired at startup, and a `QSharedMemory` segment is created. If another instance already holds the semaphore or has created the shared memory segment, the new instance exits gracefully.

### 2.2 Core Components
- **UI Layer (`src/ui/`):** Handles user interaction, input, and visual feedback using Qt Widgets.
  - **AdvancedSettingsTab navigation**: The left-side category list is a compact `QListWidget` whose stylesheet is rebuilt from `QPalette` values whenever the palette changes so it remains consistent with both light and dark themes.
  - **Runtime format selection**: Advanced Settings can defer the entire video/audio format decision until enqueue time by setting `Quality` to `Select at Runtime`; `DownloadManager` fetches format metadata and `MainWindow` presents `FormatSelectionDialog`, which enqueues one item per selected format.
- **Core Logic (`src/core/`):** Manages download queues, file operations, configuration, and external process execution.
- **Utilities (`src/utils/`):** Provides helper functions for tasks like string manipulation and URL normalization.
- **Extractor Domain Loader:** `YtDlpJsonParser` loads the extractor-domain list from the app directory for clipboard auto-paste checks in `StartTab`.
- **Auto-paste Control:** `AdvancedSettingsTab` saves `General/auto_paste_on_focus`, and `MainWindow` reacts to app focus/hover to route clipboard URLs to `StartTab` when enabled.
- **Bundled Binaries (`bin/`):** Contains the necessary executables (`yt-dlp.exe`, `ffmpeg.exe`, etc.).

### 2.4 Window/Tray Lifecycle
- The main window close action (`X`) performs an application exit after temp-file safety checks.
- The tray icon remains available for manual show/hide and quit commands, but close-to-tray behavior is not used.

### 2.3 Data Flow
1.  **Input:** User enters a URL in `StartTab`.
2.  **Validation/Expansion:** `DownloadManager` validates the URL. If it's a playlist, `PlaylistExpander` expands it into individual items.
3.  **Runtime Selection Gate:** If Advanced Settings quality is set to `Select at Runtime` for video/audio downloads, `DownloadManager` asynchronously fetches `yt-dlp` format metadata and `MainWindow` shows `FormatSelectionDialog`. Each selected format becomes its own queued item.
4.  **Queue:** Valid URLs are added to a download queue managed by `DownloadManager`.
5.  **Execution:** `DownloadManager` spawns a worker (`YtDlpWorker` or `GalleryDlWorker`) for each item.
6.  **Progress:** The worker parses `stdout` and emits progress signals (`progressUpdated`, `speedChanged`, etc.).
7.  **UI Update:** `ActiveDownloadsTab` receives signals and updates the corresponding progress bars, labels, and plays/displays a thumbnail preview on the left side of the download GUI element.
8.  **Post-Processing:** Upon success, `DownloadManager` performs post-processing (e.g., embedding track numbers for audio playlists) and moves the file to the final output directory.

## 3. Directory Structure

```
MediaDownloader/
├── CMakeLists.txt              # Build System Configuration
├── main.cpp                    # Application Entry Point
├── src/
│   ├── core/                   # Core Business Logic
│   │   ├── ConfigManager.h/cpp   # Settings persistence (INI)
│   │   ├── ArchiveManager.h/cpp  # Download history (SQLite)
│   │   ├── DownloadManager.h/cpp # Queue & Lifecycle Management
│   │   ├── YtDlpWorker.h/cpp     # yt-dlp Process Wrapper
│   │   ├── GalleryDlWorker.h/cpp # gallery-dl Process Wrapper
│   │   ├── PlaylistExpander.h/cpp # Playlist Expansion Logic
│   │   ├── SortingManager.h/cpp  # File Sorting Logic
│   │   ├── AppUpdater.h/cpp      # Application Update Logic
│   │   ├── YtDlpUpdater.h/cpp    # yt-dlp Update Logic
│   │   ├── GalleryDlUpdater.h/cpp # gallery-dl Update Logic
│   │   ├── Logger.h/cpp          # Structured Logging
│   │   └── ...
│   ├── ui/                     # User Interface (Qt Widgets)
│   │   ├── MainWindow.h/cpp      # Main Window & Signal Hub
│   │   ├── StartTab.h/cpp        # Input Tab
│   │   ├── ActiveDownloadsTab.h/cpp # Progress Tab
│   │   ├── AdvancedSettingsTab.h/cpp # Settings Tab
│   │   ├── SortingTab.h/cpp      # Sorting Rules Tab
│   │   └── ...
│   └── utils/                  # Helper Modules
│       ├── StringUtils.h/cpp   # String/URL utilities
│       ├── YtDlpJsonParser.h/cpp # Extractor-domain cache loader
│       └── ...
├── bin/                        # External Binaries
│   ├── yt-dlp.exe
│   ├── ffmpeg.exe
│   ├── ffprobe.exe
│   ├── gallery-dl.exe
│   ├── aria2c.exe
│   └── deno.exe
└── resources/                  # qrc resources (icons, extractor seed data, etc.)
```

## 4. Key Modules

### 4.1 ConfigManager (`src/core/ConfigManager.h`)
- **Responsibilities:**
  - Loads and saves application settings to `settings.ini` using `QSettings`.
  - Provides default configuration values using an internal `m_defaultSettings` map.
  - **Ensures 100% format compatibility with the Python version's INI file.**
  - Emits `settingChanged` signal when a setting is modified.
  - Rewrites `settings.ini` into a canonical layout on save so legacy sections such as `[%General]` and deprecated duplicate aliases such as `Video/quality` do not persist on disk.

### 4.2 ArchiveManager (`src/core/ArchiveManager.h`)
- **Responsibilities:**
  - Manages the `download_archive.db` SQLite database using `QtSql`.
  - Provides methods to check for existing downloads (`isInArchive`) and add new ones (`addToArchive`).
  - **Implements URL normalization logic identical to the Python version.**

### 4.3 DownloadManager (`src/core/DownloadManager.h`)
- **Responsibilities:**
  - Manages the download queue and enforces concurrency limits (`max_threads`).
  - Intercepts runtime video/audio format-selection settings, fetches format metadata asynchronously, and re-enqueues one download per chosen format ID.
  - Handles the file lifecycle (Temp -> Final).
  - Uses `yt-dlp --print after_move:filepath` as the authoritative final output path source and moves files using Qt-native rename/copy fallback for Unicode-safe, cross-volume behavior.
  - Coordinates `YtDlpWorker` and `GalleryDlWorker` instances.
  - Handles playlist expansion via `PlaylistExpander`.
  - Performs post-processing (renaming, metadata embedding).

### 4.4 YtDlpWorker (`src/core/YtDlpWorker.h`)
- **Responsibilities:**
  - Executes `yt-dlp` commands using `QProcess`.
  - Resolves bundled `yt-dlp.exe` from both app-root and `bin/` deployment layouts.
  - Forces UTF-8 process I/O environment (`PYTHONUTF8`, `PYTHONIOENCODING`) to preserve Unicode output text.
  - Emits explicit failure results when `QProcess` cannot start, preventing stuck in-progress UI states.
  - Parses `stdout` for progress percentage, speed, ETA, and JSON metadata.
  - Captures `stderr` for error reporting.

### 4.5 YtDlpUpdater (`src/core/YtDlpUpdater.h`)
- **Responsibilities:**
  - Checks GitHub for the latest `yt-dlp` nightly build.
  - Downloads and replaces the `yt-dlp.exe` binary.
  - Fetches and emits the current `yt-dlp` version.

### 4.6 SortingManager (`src/core/SortingManager.h`)
- **Responsibilities:**
  - Evaluates user-defined sorting rules against video metadata.
  - Replaces dynamic tokens (e.g., `{uploader}`) in destination paths.

### 4.7 Logger (`src/core/Logger.h`)
- **Responsibilities:**
  - Installs a Qt message handler to capture debug output.
  - Writes logs to a rotating file (`MediaDownloader.log`).

## 5. Concurrency Model
- **GUI Thread:** The main thread handles all UI updates and user interactions. **No blocking operations are allowed on this thread.**
- **Worker Threads:** `QProcess` runs external binaries asynchronously. Qt signals and slots are used for communication.

## 6. Deployment
- **Build System:** CMake.
- **Installer:** NSIS will be used to create a Windows installer (`MediaDownloader-Setup.exe`).
- **Executable Name:** The final executable will be named `MediaDownloader.exe` to ensure a seamless update from the Python version.
- **Bundling:** All dependencies (Qt runtime DLLs, binaries) will be included in the installation directory.
- **Qt Image Format Plugins:** Windows deployments must include the required `plugins/imageformats` codecs for active-download artwork and converted thumbnails, including `qjpeg`, `qpng`, `qwebp`, and `qico` (plus debug variants when available).
