# AGENTS.md - AI Contributor Guide for MediaDownloader

This document is the **canonical instruction set for all AI agents** working on the MediaDownloader project.
It defines the project's purpose, architecture, constraints, and rules for safe and effective contributions.

All agents MUST follow this document as the source of truth.

Fast Start: Keep UI responsive (no blocking I/O on GUI thread; use background threads). Preserve download lifecycle (temp -> verify -> move to completed dir). Use bundled binaries in `bin/` only; no runtime installs. Update `CHANGELOG.md` for significant changes. Update Section 3 + Quick-Reference list when files/roles change. For file locations, jump to the Quick-Reference list in Section 3.

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
- Configurable output options: audio/video quality, formats, SponsorBlock, filename sanitization
- Robust error handling with **user-friendly messages**
- File lifecycle: download into temp dir -> verify file stability -> move to completed downloads directory
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

**POLICY:** Agents MUST update this section and the "Quick-Reference" list below whenever new files are added or responsibilities shift.

The project follows a **modular, separation-of-concerns design**.

### Entry Point
- `main.pyw` - Initializes the PyQt application, configures logging, launches the main window.

### UI Layer (`ui/`)
- `ui/main_window.py` - Application shell and signal orchestrator; initializes tabs, connects global signals (like log updates), handles window state.
- `ui/tab_start.py` - Input and configuration; URL field, Download Type dropdown (Video/Audio/Formats), "Add to Queue" logic.
- `ui/tab_active.py` - Monitoring; renders active/completed downloads, handles progress updates, cancel buttons, and "Open Folder" actions.
- `ui/tab_advanced.py` - Global settings; UI for max concurrency, file paths, format selection, and binary updates.
- `ui/widgets.py` - Component library; custom styled widgets, tooltips, and reusable UI elements.

### Core Logic (`core/`)
- `core/config_manager.py` - State persistence; reads/writes `settings.json`, provides default config values.
- `core/download_manager.py` - "Brain"; URL validation (Tier 1 regex, Tier 2 probe), queue management (max 4 startup / 8 runtime), lifecycle (Queue -> Active -> Completed), file ops (move `temp` -> `output` on completion).
- `core/yt_dlp_worker.py` - "Muscle"; runs `yt-dlp` subprocesses, parses stdout for progress % and speed, handles thumbnail embedding and JSON metadata extraction.
- `core/app_updater.py` - Self-maintenance; checks GitHub Releases API for application updates.
- `core/playlist_expander.py` - Pre-processing; takes a playlist URL and yields individual video URLs.
- `core/logger_config.py` - Diagnostics; configures Python `logging`, handles `sys.stderr` redirection to GUI console.
- `core/file_ops_monitor.py` - File system monitoring logic.

### Utilities (`utils/`)
- `utils/` - Helper modules such as cookie handling.

### Quick-Reference: Where is X?

- URL Validation: `core/download_manager.py` (Tier 1 regex, Tier 2 probe).
- Download Queue: `core/download_manager.py` (concurrency semaphores and queue state).
- Process Execution: `core/yt_dlp_worker.py` (wraps `yt-dlp` subprocess; parses stdout/stderr).
- Progress Bars: `ui/tab_active.py` (updates UI based on worker signals).
- App Updates: `core/app_updater.py` (checks GitHub for new app versions).
- Settings/Config: `core/config_manager.py` (handles `settings.json` I/O).
- External Downloader: `ui/tab_advanced.py`, `core/yt_dlp_worker.py` (`aria2` integration).
- File Moving: `core/download_manager.py` (moves files from `temp_downloads` to final output).
- Logging Setup: `core/logger_config.py` (configures logging and captures stderr).

---

## 4. Bundled / Compiled Dependencies (Non-Negotiable)

When compiled, the application MUST remain **standalone**.

Bundled binaries (located in `bin/`):
- `yt-dlp`
- `ffmpeg`
- `ffprobe`
- `aria2c`

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
- **Update AGENTS.md** (Architecture & Quick-Reference) if you add files or change core logic locations

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

The app entry point is main.pyw

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

- Project follows PEP8

---

## 11. Changelog Policy

All significant changes to the application MUST be recorded in `CHANGELOG.md`.
Agents should not log changes in `AGENTS.md` beyond architectural updates required by Section 3.

---

## 12. Agent Task Persistence Protocol

To ensure continuity and seamless handover between different AI agents working on this project, all agents MUST adhere to the following protocol. This section is the **live tracker** for current development tasks.

### Core Principle

This section of `AGENTS.md` serves as the single source of truth for the current state of any ongoing task. It allows any agent to understand the work that has been done and what remains.

### Procedure

1.  **Declare Your Plan:** Before beginning any complex task (e.g., feature implementation, multistep bug fix), the agent MUST outline its plan as a Markdown checklist under the "Current Tasks" heading below.
2.  **Track Progress:** The agent MUST update the checklist in real-time as it works. Mark tasks as complete by checking them (`- [x]`). Add or modify sub-tasks as the plan evolves.
3.  **Handoff/Resumption:** Before stopping its work session, the agent must ensure the checklist accurately reflects the current status. A new agent resuming work will consult this section first to pick up where the previous one left off.
4. **Cleanup on Completion:** Once a top-level Task (and all its sub-tasks) is 100% checked off, the agent MUST remove that task from the "Current Tasks" section to keep the context window lean. Important architectural notes from the task should be moved to a "Project Log" section if necessary; otherwise, delete it.

### Current Tasks

