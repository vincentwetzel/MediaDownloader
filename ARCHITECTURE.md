# MediaDownloader Architecture

## 1. Overview
MediaDownloader is a Python-based desktop application built with **PyQt6** that provides a graphical interface for downloading media using **yt-dlp**. The application is designed to be modular, responsive, and self-contained when compiled.

## 2. System Design

### 2.1 Core Components
- **UI Layer (`ui/`):** Handles user interaction, input validation, and visual feedback.
- **Core Logic (`core/`):** Manages download queues, file operations, configuration, and external process execution.
- **Utilities (`utils/`):** Provides helper functions for specific tasks like cookie extraction.
- **Bundled Binaries (`bin/`):** Contains the necessary executables (`yt-dlp`, `ffmpeg`, etc.) required for operation.

### 2.2 Data Flow
1.  **Input:** User enters a URL in `ui/tab_start.py`.
2.  **Validation:** `core/download_manager.py` validates the URL (regex/probe).
3.  **Queue:** Valid URLs are added to a download queue managed by `core/download_manager.py`.
4.  **Execution:** `core/yt_dlp_worker.py` spawns a `yt-dlp` subprocess for each item.
5.  **Progress/Artwork:** `yt-dlp` stdout is parsed by `core/yt_dlp_worker.py` and emitted via signals to `ui/tab_active.py`; thumbnail image paths are emitted from worker -> `core/download_manager.py` -> `ui/tab_active.py` and stored only in a session-scoped temp cache. In `ui/tab_active.py`, audio-only thumbnails are center-cropped to square before rendering to align with audio artwork conventions.
6.  **Playlist Track Tags (Audio):** `core/playlist_expander.py` extracts `playlist_index` during expansion, `ui/main_window.py` forwards it per-entry, and `core/yt_dlp_worker.py` applies track metadata through `core/playlist_track_tagger.py` with bundled `ffmpeg`.
6.  **Completion:** Upon success, files are moved from `temp_downloads` to the final output directory.

## 3. Directory Structure

```
MediaDownloader/
├── main.pyw                    # Application Entry Point
├── core/                        # Business Logic
│   ├── config_manager.py       # Settings persistence (JSON)
│   ├── download_manager.py     # Queue & Lifecycle Management
│   ├── yt_dlp_worker.py        # Subprocess Wrapper & Parsing
│   ├── yt_dlp_args_builder.py  # CLI Argument Construction
│   ├── updater.py              # Self-Update Logic
│   ├── playlist_expander.py    # Playlist Handling
│   ├── playlist_track_tagger.py # Playlist Track Tagging (Audio)
│   ├── logger_config.py        # Logging Configuration
│   ├── file_ops_monitor.py     # File System Monitoring
│   └── sorting_manager.py      # File Sorting Logic
├── ui/                          # User Interface
│   ├── main_window.py          # Main Window & Signal Hub
│   ├── tab_start.py            # Input Tab
│   ├── tab_active.py           # Progress Tab
│   ├── tab_advanced.py         # Settings Tab
│   ├── tab_sorting.py          # Sorting Rules Tab
│   └── widgets.py              # Reusable Components
├── utils/                       # Helper Modules
│   └── cookies.py              # Browser Cookie Extraction
├── bin/                         # External Binaries
│   ├── yt-dlp.exe
│   ├── ffmpeg.exe
│   └── ...
├── tests/                       # Testing
│   └── auto_test.py            # Integration Tests
├── pyproject.toml              # Project Metadata
└── main.spec                   # PyInstaller Config
```

## 4. Key Modules

### 4.1 Download Manager (`core/download_manager.py`)
- **Responsibilities:**
  - Validates URLs.
  - Manages the download queue (concurrency limits).
  - Handles file lifecycle (Temp -> Final).
  - Applies playlist index filename prefixes (`NN - `) for audio playlist items during final move.
  - Coordinates with `yt_dlp_worker.py`.

### 4.2 YT-DLP Worker (`core/yt_dlp_worker.py`)
- **Responsibilities:**
  - Executes `yt-dlp` commands.
  - Parses stdout for progress percentage and speed.
  - Captures stderr for error reporting.
  - Handles thumbnail embedding.
  - Applies playlist track number metadata for audio playlist entries.

### 4.3 Playlist Track Tagger (`core/playlist_track_tagger.py`)
- **Responsibilities:**
  - Applies `track` / `tracknumber` tags to completed audio files using `ffmpeg` remux (`-c copy`).
  - For `.opus` outputs, retries metadata remux with audio-only stream mapping when ffmpeg rejects embedded cover-art streams.
  - Normalizes playlist track values to zero-padded formatting for single-digit indices.
  - Safely skips sidecar/non-media files and leaves downloads intact on tagging failures.

### 4.4 Config Manager (`core/config_manager.py`)
- **Responsibilities:**
  - Loads and saves application settings to `settings.json`.
  - Provides default configuration values.

### 4.5 Updater (`core/updater.py`)
- **Responsibilities:**
  - Checks GitHub Releases API for updates.
  - Downloads and installs updates.
  - Manages `yt-dlp` binary updates.

## 5. Concurrency Model
- **GUI Thread:** The main thread handles all UI updates. Blocking operations are strictly prohibited.
- **Worker Threads:** `QThread` or `threading.Thread` are used for:
  - `yt-dlp` subprocess execution.
  - File operations (moving/renaming).
  - Network requests (update checks).
- **Signals & Slots:** PyQt signals are used to communicate between worker threads and the GUI thread, ensuring thread safety.

## 6. Deployment
- **PyInstaller:** Used to package the application into a single executable or directory.
- **NSIS:** Used to create a Windows installer (`MediaDownloader-Setup.exe`).
- **Bundling:** All dependencies (Python runtime, libraries, binaries) are included in the distribution.

## 7. Future Considerations
- **Plugin System:** Allow third-party extensions for additional sites or post-processing.
- **Remote Control:** API for controlling downloads remotely.
- **Cross-Platform UI:** Better support for Linux/macOS native look and feel.
