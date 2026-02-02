# AGENTS.md — AI Contributor Guide for MediaDownloader

This document is the **canonical instruction set for all AI agents** working on the MediaDownloader project.
It defines the project’s purpose, architecture, constraints, and rules for safe and effective contributions.

All agents MUST follow this document as the source of truth.

---

## 1. Project Overview

**MediaDownloader** is a Python-based desktop application built with **PyQt6** that allows users to download media
(primarily video and audio) from online platforms using **yt-dlp**.

Key goals:
- Provide a **stable, user-friendly GUI**
- Support **concurrent downloads**, playlists, and advanced configuration
- Be **self-contained** when compiled (no external dependency installs)
- Prioritize **UI responsiveness**, robustness, and clear error handling

The target platform is primarily **Windows**, but path handling and logic should remain cross-platform.

---

## 2. Core Functionality (What Must Not Break)

Agents MUST preserve and respect the following behaviors:

- Media downloading via `yt-dlp`
- Concurrent download management with user-defined limits (capped at 4 on app startup)
- Playlist expansion and processing
- **Format inspection**: Users can view available download formats for a URL via "View Formats" option in Download Type dropdown
- Configurable output options:
  - audio/video quality
  - formats
  - SponsorBlock
  - filename sanitization
- Robust error handling with **user-friendly messages**
- File lifecycle:
  - download into a temporary directory
  - verify file stability
  - move to completed downloads directory
- Metadata embedding (title, artist, etc.) and thumbnail embedding for both audio and video downloads
- Thumbnail downloading and embedding using `ffmpeg`
- Responsive GUI at all times (no blocking I/O on the main thread)
- Comprehensive logging (including redirected stderr)
- Enforced output directory selection on first run
- Automatic creation of `temp_downloads` as a subdirectory of the output folder
- In-app updating of the `yt-dlp` executable (stable and nightly channels)
- Optional JavaScript runtime support (Node.js / Deno) for anti-bot challenges
- Full, non-truncated filenames using video title and uploader name

---

## 3. Architecture Overview

The project follows a **modular, separation-of-concerns design**.

### Entry Point
- `main.pyw`
  - Initializes the PyQt application
  - Configures logging
  - Launches the main window

### UI Layer (`ui/`)
- `main_window.py`
  - Main `QMainWindow`
  - Hosts the tab system
  - Orchestrates UI ↔ core interactions
- `tab_start.py`
  - URL input and download initiation
- `tab_active.py`
  - Displays active and completed downloads
- `tab_advanced.py`
  - Advanced settings UI
- `widgets.py`
  - Shared custom PyQt widgets

### Core Logic (`core/`)
- `config_manager.py`
  - Loads, saves, and validates settings
- `download_manager.py`
  - Queues downloads
  - Manages concurrency
  - Handles post-download file operations
- `yt_dlp_worker.py`
  - Runs `yt-dlp` in a background thread
  - Parses progress and metadata
  - Embeds thumbnails
- `playlist_expander.py`
  - Detects and expands playlist URLs
- `logger_config.py`
  - Logging setup and stderr redirection
- `file_ops_monitor.py`
  - File system monitoring logic

### Utilities (`utils/`)
- Helper modules such as cookie handling

---

## 4. Bundled / Compiled Dependencies (Non-Negotiable)

When compiled, the application MUST remain **standalone**.

Bundled binaries (located in `bin/`):
- `yt-dlp`
- `ffmpeg`
- `ffprobe`

Agents MUST NOT:
- Remove bundling assumptions
- Introduce runtime dependency downloads
- Require the user to install these separately

---

## 5. Development Stack

- Python 3.x
- PyQt6
- `requests`
- Standard library: `json`, `subprocess`, `threading`
- PyInstaller (build configuration in `main.spec`)

Dependencies are defined in `requirements.txt`.

---

## 6. Agent Rules (Read Carefully)

### You MUST:
- Keep the UI responsive (no blocking calls on the GUI thread)
- Use background threads for I/O-heavy operations
- Preserve public behavior and settings formats
- Maintain clear, user-facing error messages
- Respect existing file lifecycle and temp directory logic
- Add logging for non-trivial changes

### You MUST NOT:
- Change download paths, archive formats, or config formats without explicit intent
- Introduce new dependencies casually
- Break compiled/standalone behavior
- Assume network availability beyond `yt-dlp` execution

---

## 7. Platform-Specific Constraints

- On Windows **compiled builds only**, subprocesses MUST suppress console windows
- When running from source, console output should remain visible for debugging
- Use `sys.frozen` to distinguish compiled vs source execution

---

## 8. Running the Application (From Source)

To run the MediaDownloader application from source:

    python main.pyw

(On Windows, `pythonw main.pyw` may be used to suppress the console window.)

---

## 9. Testing

The project includes an automated integration test:

- `tests/auto_test.py`
  - Launches the application
  - Queues predefined YouTube URLs
  - Exits after a timeout

To run the automated test:

    python tests/auto_test.py

No formal unit test framework is currently in use.

Agents should favor **behavior-preserving changes** and manual validation via this test.

---

## 10. Code Style & Comments

- Follow standard Python best practices
- Comment complex logic with an emphasis on **why**, not just what
- Especially document:
  - file movement logic
  - subprocess invocation
  - race condition mitigations

---

## 11. Change Log (Agent Modifications)

Significant agent-driven changes should be summarized here with:
- Date
- High-level description
- Rationale (especially for bug fixes and refactors)

This section exists to preserve historical context for future agents.

### 2026-02-02
- **Fixed metadata embedding for videos**: Added `--embed-metadata` and `--embed-thumbnail` flags to video downloads (previously only applied to audio).
- **Capped max concurrent downloads at 4 on app startup**: While users can temporarily set higher concurrency (up to 8) in the UI, any value above 4 reverts to 4 when the app restarts. This prevents users from accidentally launching with overly aggressive concurrency.
- **Added "View Formats" feature**: Users can now select "View Formats" from the Download Type dropdown to inspect all available download formats for a URL without downloading. This runs `yt-dlp -F <URL>` and displays results in a dialog.
- **Dynamic Download button text**: The main Download button now changes its label based on the selected Download Type (e.g., "Download Video", "Download Audio", "View Formats") to provide clear user feedback.
- **Added comprehensive mouseover tooltips**: All UI elements now have helpful tooltips explaining their purpose, including buttons, dropdowns, checkboxes, and labels across all three tabs (Start, Active, Advanced).
- **Early unsupported-URL detection**: Added a quick background validation step before creating Active Downloads UI elements. If `yt-dlp` cannot quickly extract metadata for a URL, the app will report the URL as unsupported and will not create the download UI entry, preventing wasted UI artifacts and faster user feedback.
 - **Early unsupported-URL detection**: Added a quick background validation step before creating Active Downloads UI elements. If `yt-dlp` cannot quickly extract metadata for a URL, the app will report the URL as unsupported and will not create the download UI entry, preventing wasted UI artifacts and faster user feedback.
 - **Extractor index & host heuristics**: At startup the app now attempts to build a local extractor index (using the `yt_dlp` python module when available) and uses it to rapidly determine whether a host is supported. This reduces latency for common hosts like YouTube by avoiding the slower metadata probes.
