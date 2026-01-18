# Gemini AI - Project Understanding Document for MediaDownloader

This document provides an overview of the `MediaDownloader` project, designed to assist the Gemini AI agent in understanding its structure, purpose, and operational details for effective task execution.

## 1. Project Overview

`MediaDownloader` is a Python-based desktop application built with PyQt6, designed to facilitate the downloading of media content (primarily videos) from various online sources. It leverages the `yt-dlp` tool for media extraction and downloading, offering features such as concurrent downloads, playlist handling, configurable output settings (e.g., audio/video quality, format), and post-processing like thumbnail embedding using `ffmpeg`.

The application features a graphical user interface with multiple tabs for initiating downloads, monitoring active downloads, and configuring advanced settings.

## 2. Core Functionality

*   **Media Downloading:** Utilizes `yt-dlp` to download videos and audio from supported platforms.
*   **Concurrency Management:** Manages multiple simultaneous downloads based on user-defined limits.
*   **Playlist Support:** Expands and processes URLs that are identified as playlists.
*   **Configurable Settings:** Allows users to customize download paths, video/audio quality, formats, filename sanitization, SponsorBlock integration, and more.
*   **File Organization:** Moves downloaded files from a temporary directory to a designated completed downloads directory upon completion, with robust error recovery and file stability checks.
*   **Thumbnail Embedding:** Downloads and embeds the best available thumbnail into the downloaded media file using `ffmpeg`.
*   **User Interface:** Provides a responsive GUI for interaction, status monitoring, and settings management.
*   **Logging:** Comprehensive logging system that captures application events and redirects standard error (stderr) to a log file for debugging purposes.
*   **Automatic Path Configuration:** Enforces the selection of an output directory on first run and automatically configures a temporary download directory as a subdirectory (`temp_downloads`) of the chosen output folder. This path handling is cross-platform compatible.
*   **In-App Updates:** Provides functionality to update the `yt-dlp` executable directly from the Advanced Settings tab, supporting both stable and nightly channels. The UI displays the current version and indicates if it is a stable or nightly build.

## 3. Architecture Overview

The project follows a modular structure, separating concerns into UI, core logic, and utilities.

*   **`main.pyw`**: The primary entry point of the application. It initializes the PyQt application, sets up logging, and launches the `MediaDownloaderApp` main window.
*   **`ui/` directory**: Contains all PyQt6-related UI components:
    *   `main_window.py`: The main application window (`QMainWindow`) which hosts a `QTabWidget` for navigation. It orchestrates interactions between UI elements and core logic components.
    *   `tab_start.py`: (Inferred) Likely handles URL input and initiation of downloads.
    *   `tab_active.py`: (Inferred) Displays and manages active and completed downloads.
    *   `tab_advanced.py`: (Inferred) Provides an interface for configuring application settings.
    *   `widgets.py`: (Inferred) Contains custom PyQt widgets used across the UI.
*   **`core/` directory**: Houses the core business logic:
    *   `config_manager.py`: Manages loading, saving, and accessing application settings.
    *   `download_manager.py`: Manages the overall download process, including queuing, concurrency, and post-download file operations. It uses `DownloadWorker` instances.
    *   `yt_dlp_worker.py`: A `QThread`-based worker responsible for executing `yt-dlp` commands in a separate thread. Handles subprocess management, progress reporting, metadata fetching, and thumbnail embedding.
    *   `playlist_expander.py`: Logic for identifying and expanding playlist URLs into individual media URLs.
    *   `logger_config.py`: Configures the application's logging system, including stderr redirection.
    *   `file_ops_monitor.py`: (Inferred) Monitors file system operations.
*   **`utils/` directory**: Contains various utility scripts, e.g., `cookies.py` (likely for managing browser cookies for authenticated downloads).
*   **`logs/` directory**: Stores application logs.
*   **`temp_test/` directory**: Likely a temporary directory for testing purposes.
*   **`tests/` directory**: Contains an automated integration test script.

## 4. Key Technologies and External Dependencies

*   **Python 3.x**: The primary programming language.
*   **PyQt6**: The GUI toolkit used for building the desktop application.
*   **`yt-dlp`**: An external command-line program essential for media downloading. It must be installed and accessible in the system's PATH, or otherwise discoverable by the application (as handled by `yt_dlp_worker.py`).
*   **`ffmpeg`**: An external command-line program used for media processing, specifically for embedding thumbnails into downloaded files. Must be installed and accessible in the system's PATH.
*   **`ffprobe`**: Part of `ffmpeg`, used to inspect media file properties.
*   **`requests`**: Python library used for making HTTP requests, specifically for downloading thumbnails.
*   **`json`**: Standard Python library for JSON processing.
*   **`subprocess`**: Standard Python library for spawning new processes, used to run `yt-dlp` and `ffmpeg`.
*   **`threading`**: Standard Python library for managing concurrent execution.

## 5. Running the Application

To run the `MediaDownloader` application:

```bash
python main.pyw
```
(On Windows, `pythonw main.pyw` might be preferred to run without a console window.)

## 6. Testing

The project includes an automated integration test:

*   **`tests/auto_test.py`**: This script launches the `MediaDownloaderApp`, configures it to download a predefined set of YouTube URLs, and then automatically quits after a timeout. It serves as a functional test to ensure basic download operations and UI responsiveness.

To run the automated test:

```bash
python tests/auto_test.py
```

*Note: No formal unit test suite (e.g., using `unittest` or `pytest`) was identified in the project structure.*

## 7. Development Environment

*   **Python Version:** Python 3.x
*   **Dependency Management:** Project dependencies are listed in `requirements.txt`. Key Python dependencies include `PyQt6` and `requests`. External executables `yt-dlp` and `ffmpeg` are critical and must be installed separately.

## 8. Linting & Formatting

No explicit linting or formatting configuration files (e.g., `.ruff.toml`, `pyproject.toml` sections for linters) were found. It is assumed that standard Python best practices are followed, or linting is handled externally/manually.

## 9. Code Comments

Code comments are used to explain complex logic, especially in areas like file operations (`download_manager.py`) and subprocess management (`yt_dlp_worker.py`). The style aims for clarity regarding *why* something is done.