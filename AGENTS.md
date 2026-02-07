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

**POLICY:** Agents MUST update this section and the "Quick-Reference" table below whenever new files are added or responsibilities shift.

The project follows a **modular, separation-of-concerns design**.

### Entry Point
- `main.pyw`
  - Initializes the PyQt application
  - Configures logging
  - Launches the main window

### UI Layer (`ui/`)
- `main_window.py`
  - **Role**: Application shell and signal orchestrator.
  - **Key Responsibilities**: Initializes tabs, connects global signals (like log updates), handles window state.
- `tab_start.py`
  - **Role**: Input and Configuration.
  - **Key Responsibilities**: URL text field, Download Type dropdown (Video/Audio/Formats), "Add to Queue" button logic.
- `tab_active.py`
  - **Role**: Monitoring.
  - **Key Responsibilities**: Renders the list of active/completed downloads, handles progress bar updates, cancel buttons, and "Open Folder" actions.
- `tab_advanced.py`
  - **Role**: Global Settings.
  - **Key Responsibilities**: UI for max concurrency, file paths, format selection, and binary updates.
- `widgets.py`
  - **Role**: Component Library.
  - **Key Responsibilities**: Custom styled widgets, tooltips, and reusable UI elements.

### Core Logic (`core/`)
- `config_manager.py`
  - **Role**: State Persistence.
  - **Key Responsibilities**: Reads/writes `settings.json`, provides default config values.
- `download_manager.py`
  - **Role**: The "Brain" of the operation.
  - **Key Responsibilities**:
    - **URL Validation**: Tier 1 (Regex) and Tier 2 (Probe) checks.
    - **Queue Management**: Enforces concurrency limits (max 4 startup / 8 runtime).
    - **Lifecycle**: Orchestrates the flow from Queue -> Active -> Completed.
    - **File Ops**: Moves files from `temp` to `output` upon completion.
- `yt_dlp_worker.py`
  - **Role**: The "Muscle".
  - **Key Responsibilities**:
    - Runs `yt-dlp` subprocesses.
    - **Parsing**: Regex parsing of `yt-dlp` stdout for progress % and speed.
    - **Metadata**: Handles thumbnail embedding and JSON metadata extraction.
 - `app_updater.py`
   - **Role**: Self-Maintenance.
   - **Key Responsibilities**: Checks GitHub Releases API for application updates.
- `playlist_expander.py`
  - **Role**: Pre-processing.
  - **Key Responsibilities**: Takes a playlist URL and yields individual video URLs.
- `logger_config.py`
  - **Role**: Diagnostics.
  - **Key Responsibilities**: Configures Python `logging`, handles `sys.stderr` redirection to GUI console.
- `file_ops_monitor.py`
  - File system monitoring logic

### Utilities (`utils/`)
- Helper modules such as cookie handling

### Quick-Reference: Where is X?

| Feature / Logic | Primary File(s) | Notes |
| :--- | :--- | :--- |
| **URL Validation** | `core/download_manager.py` | Contains Tier 1 (Regex) and Tier 2 (Probe) logic. |
| **Download Queue** | `core/download_manager.py` | Manages concurrency semaphores and queue state. |
| **Process Execution** | `core/yt_dlp_worker.py` | Wraps `yt-dlp` subprocess calls; parses stdout/stderr. |
| **Progress Bars** | `ui/tab_active.py` | Updates UI based on worker signals. |
| **App Updates** | `core/app_updater.py` | Checks GitHub for new app versions. |
| **Settings/Config** | `core/config_manager.py` | Handles `settings.json` I/O. |
| **External Downloader** | `ui/tab_advanced.py`, `core/yt_dlp_worker.py` | `aria2` integration. |
| **File Moving** | `core/download_manager.py` | Moves files from `temp_downloads` to final output. |
| **Logging Setup** | `core/logger_config.py` | Configures `logging` and captures stderr. |

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
- **Extractor index & host heuristics**: At startup the app now attempts to build a local extractor index (using the `yt_dlp` python module when available) and uses it to rapidly determine whether a host is supported. This reduces latency for common hosts like YouTube by avoiding the slower metadata probes.

### 2026-02-03
- **Implemented Tiered URL Validation**: Refined the early supported-URL detection into a two-tier system to balance speed and accuracy.
    - **Tier 1 (Fast-Track)**: The app now performs an immediate string/regex check for high-traffic domains (e.g., `youtube.com`, `youtu.be`, `music.youtube.com`). These are accepted instantly to provide zero-latency UI feedback.
    - **Tier 2 (Metadata Probe)**: For less-common domains, the app initiates a background `yt-dlp --simulate` call. The "Active Download" UI entry is only generated if this probe confirms the URL is a valid target.
- **Rationale**: This hybrid approach eliminates the "verification lag" for the most frequent use cases (YouTube) while preventing the UI from being cluttered with invalid or unsupported URLs for niche sites.

### 2026-02-04
- **Configurable Output Filename Pattern**: Added a new GUI textbox in the Advanced Settings tab to allow users to define the output filename pattern using `yt-dlp` template variables.
    - **Default**: `%(title)s [%(uploader)s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s`
    - **Validation**: Basic validation ensures balanced parentheses and brackets before saving.
    - **Reset**: Users can easily revert to the default pattern.
    - **Integration**: The `DownloadManager` now pulls this pattern from the configuration instead of using a hardcoded string.

### 2026-02-06
- **Fixed ConfigParser Interpolation Error**: Disabled interpolation in `ConfigManager` to prevent `configparser` from attempting to interpret `yt-dlp` template variables (like `%(title)s`) as configuration references. This fixed a crash on startup.

### 2026-02-07
- **Implemented Application Self-Update Mechanism**: Added logic to check for updates from GitHub Releases, download the update, and apply it.
    - **Update Check**: Queries GitHub API for the latest release tag.
    - **Download**: Downloads the update asset (exe/msi) to a temporary location.
    - **Apply**: If it's an installer, launches it. If it's a binary replacement, uses a batch script to swap the executable and restart.
    - **UI Integration**: Added a prompt in the main window when an update is available, with options to update now or view the release page.

### 2026-02-08
- **Added Global Download Speed Indicator**: Implemented a real-time download speed indicator at the bottom of the main application window.
    - **Implementation**: Uses `psutil.Process().io_counters().read_bytes` to measure the total I/O read throughput of the application process.
    - **UI Update**: Added a `QLabel` to the bottom of `MediaDownloaderApp` to display the total speed.
    - **Aggregation**: Implemented a timer in `MediaDownloaderApp` to calculate the speed difference every second.
    - **Rationale**: Provides a more accurate and simpler measure of actual data throughput compared to parsing `yt-dlp` stdout, and avoids per-file tracking complexity.

### 2026-02-09
- **Added Theme Selection**: Added a dropdown in Advanced Settings to switch between "System", "Light", and "Dark" themes.
    - **Implementation**: Updates `main.pyw` to apply the selected theme on startup.
    - **Dark Mode**: Uses `pyqtdarktheme` if available, otherwise falls back to system style.
    - **Persistence**: Saves the user's choice in `settings.json`.
    - **Dynamic Switching**: Theme changes are applied immediately without requiring a restart.
    - **Robustness**: Added checks for `setup_theme` vs `load_stylesheet` to support different versions of `pyqtdarktheme` or compatible libraries.
    - **System Theme Detection**: Added Windows registry check to correctly detect system theme when "System" (auto) is selected, fixing an issue where it defaulted to dark mode.

### 2026-02-10
- **Fixed Global Download Speed Indicator**: Modified the download speed indicator to include I/O from child processes.
    - **Implementation**: The speed calculation now iterates through the main process and all its children (`psutil.Process().children(recursive=True)`), summing their `io_counters` to get a total I/O throughput.
    - **Rationale**: This provides a more accurate download speed measurement, as `yt-dlp` runs in separate subprocesses whose I/O was not previously being tracked.

### 2026-02-11
- **Fixed Global Download Speed Indicator (Again)**: The previous fix for the download speed indicator was flawed and caused it to display `-- MB/s`. The calculation logic has been rewritten to be more robust and now correctly sums the I/O from the main process and all its children.
    - **Implementation**: The `_get_total_io_counters` function now simply sums the `read_bytes` from the main process and all its children, with proper error handling for terminated processes.
    - **Rationale**: This simpler implementation is more resilient and provides an accurate total download speed.

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

