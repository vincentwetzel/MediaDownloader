# MediaDownloader Specification

## 1. Introduction
MediaDownloader is a desktop application designed to simplify the process of downloading video and audio content from various online platforms. It leverages `yt-dlp` for robust extraction capabilities and provides a user-friendly PyQt6 graphical interface.

## 2. Goals & Scope
- **Primary Goal:** Provide a stable, standalone Windows application for downloading media without requiring command-line knowledge.
- **Scope:**
  - Support for single video and playlist downloads.
  - Audio extraction and video format selection.
  - Concurrent download management.
  - Self-contained deployment (bundled binaries).

## 3. User Personas
- **Casual User:** Wants to download a YouTube video as an MP3 with minimal configuration.
- **Power User:** Wants to archive entire playlists, configure specific resolutions, use SponsorBlock, and manage file naming conventions.

## 4. Functional Requirements

### 4.1 Download Management
- **URL Input:** Users must be able to paste URLs from supported platforms.
- **Validation:** The system must validate URLs using regex (Tier 1) and probing (Tier 2) before queuing.
- **Queue System:**
  - Users can add multiple items to a queue.
  - The application must support concurrent downloads (default cap: 4).
  - Users can cancel active downloads.
- **Playlists:** The system must detect playlist URLs and offer to expand them into individual download items.
- **Playlist Audio Track Tags:** For audio playlist downloads, the system must write each entry's playlist position into media tags (`track`/`tracknumber`) so files preserve album order in music players.
  - For `.opus` outputs, tagging must update tags in-place without remuxing so embedded artwork is preserved.
  - Single-digit playlist indices should be written as zero-padded values (`01`..`09`).
  - Audio playlist output filenames should be prefixed with the zero-padded playlist index (`NN - `) before final move to completed output.

### 4.2 Configuration & Customization
- **Download Types:**
  - Video (with audio).
  - Audio Only (conversion to MP3, M4A, etc.).
  - View Formats (inspect available streams).
- **Quality Control:** Users can select target resolutions (e.g., 1080p, 4k) and audio quality.
- **Output Management:**
  - Configurable output directory.
  - Automatic creation of a `temp_downloads` subdirectory for in-progress files.
  - File naming sanitization and metadata embedding.
- **Advanced Options:**
  - SponsorBlock integration (skip/remove segments).
  - Thumbnail embedding.
  - `.opus` audio outputs must preserve yt-dlp embedded artwork and avoid incompatible generic attached-pic remux rewrites.
  - Browser cookie import for age-restricted content.

### 4.3 Application Lifecycle
- **Startup:**
  - Load settings from `settings.json`.
  - Check for application updates via GitHub Releases.
  - Verify presence of bundled binaries (`yt-dlp`, `ffmpeg`, etc.).
- **Updates:**
  - Self-update capability for the application itself.
  - In-app update capability for the `yt-dlp` binary (Stable/Nightly channels).

### 4.4 User Interface
- **Responsiveness:** The UI must remain responsive during all operations. Heavy tasks must run on background threads.
- **Feedback:**
  - Real-time progress bars for downloads.
  - Thumbnail preview image for each active/queued download item when metadata provides a thumbnail URL.
  - For audio-only downloads, thumbnail previews should be center-cropped to square artwork for consistent album-art presentation.
  - Thumbnail preview files must be treated as session-temporary UI cache data and removed on application exit.
  - Console log view for `yt-dlp` output (stdout/stderr).
  - Visual indicators for success, error, or cancellation.
  - Footer contact controls should include a clickable developer Discord icon next to "Contact Developer", opening the invite link in the default browser with tooltip text "Developer Discord".

## 5. Non-Functional Requirements
- **Performance:** Minimal resource usage when idle. Efficient thread management for concurrent downloads.
- **Reliability:**
  - Downloads should not corrupt existing files.
  - The application should handle network interruptions gracefully where possible.
  - Application log files must use rotation to prevent unbounded disk usage growth during long-running sessions.
- **Portability:**
  - The application must be compilable to a standalone executable (PyInstaller).
  - No requirement for the user to install Python or FFmpeg separately.
- **Security:**
  - No execution of arbitrary remote code (beyond the self-updater verifying signatures/hashes if implemented).
  - Safe handling of file paths to prevent directory traversal.

## 6. Constraints
- **Platform:** Primary support for Windows (compiled). Source code compatible with Linux/macOS.
- **Dependencies:** Must use bundled binaries in `bin/` folder.
- **Modularity:** Codebase must adhere to the 600-line limit per file to ensure maintainability.
