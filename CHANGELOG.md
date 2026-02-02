# Changelog

All notable changes to MediaDownloader will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- Fixed critical app crash on startup caused by corrupted method merging in `main_window.py`
- Fixed unhandled exception in version fetch from daemon thread (disabled for now pending future refactor)
- Added robust error handling around signal emission in background threads

## [0.0.1] - 2026-02-02

### Added
- Initial release of MediaDownloader
- PyQt6-based GUI for downloading media via yt-dlp
- Support for 1000+ websites (YouTube, TikTok, Instagram, etc.)
- Playlist detection and expansion
- Concurrent download management with user-configurable limits (capped at 4 on startup)
- Advanced download options:
  - Audio/video quality selection
  - Format filtering by codec
  - SponsorBlock integration for automatic segment removal
  - Filename sanitization and customization
- Metadata and thumbnail embedding for videos and audio
- Browser cookie integration for age-restricted content
- Optional JavaScript runtime support (Deno/Node.js) for anti-bot challenges
- GitHub-based auto-update system:
  - Automatic release checking on app startup
  - Silent installer-based updates via NSIS
  - Manual update check button in Advanced Settings
  - Configurable auto-check toggle (persisted to settings)
  - Changelog display before updating
- Responsive UI with comprehensive tooltips
- File lifecycle management:
  - Download to temporary directory
  - File stability verification
  - Move to completed downloads directory
- Download archive tracking (prevent re-downloads)
- Robust error handling with user-friendly messages
- Comprehensive logging to file and console
- Enforced output directory selection on first run
- In-app yt-dlp version management:
  - Manual update button with stable/nightly channel selection
  - Automatic version display
- NSIS-based Windows installer for standalone distribution
- PyInstaller bundled with all dependencies (yt-dlp, ffmpeg for Windows/Linux/macOS)

### Fixed
- Signal handler connection for file move operations (worker finished signal now properly invokes handler)
- Snapshot fallback file detection by cleaning temp directory on test start
- Early URL validation to prevent wasted UI artifacts for unsupported hosts

### Technical Details
- Built with PyQt6 for responsive, cross-platform UI
- Uses yt-dlp for reliable media downloading
- FFmpeg for video/audio processing and metadata embedding
- Background thread workers for non-blocking I/O operations
- Modular architecture separating UI, core logic, and utilities
- Git-ignored `/bin/` directory with automated binary download script (`download_binaries.ps1`)
- NSIS installer supporting silent installation (`/S /D=<path>` flags) for seamless updates
- Semantic versioning in `core/version.py` as single source of truth

## Future Roadmap

- [ ] Automatic nightly builds and releases
- [ ] Rollback to previous version option
- [ ] Delta updates (download only changed files)
- [ ] Silent automatic updates (no prompt, happens in background)
- [ ] Custom video/audio post-processing
- [ ] Subtitle/caption download integration
- [ ] Search and download by query
- [ ] Download statistics and analytics dashboard
- [ ] Cross-platform support (native builds for macOS/Linux)
