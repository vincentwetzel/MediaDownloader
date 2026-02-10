# MediaDownloader

A lightweight PyQt6-based desktop application for downloading media (video and audio) from online platforms using **yt-dlp**, with automatic updates via GitHub Releases.

## Features

- 🎬 **Download Video & Audio** — Support for YouTube, TikTok, Instagram, and 1000+ other sites via yt-dlp
- 🎵 **Audio Extraction** — Extract audio as MP3, M4A, opus, or other formats
- 📋 **Playlist Support** — Download entire playlists with configurable behavior
- 🎨 **Advanced Settings** — Quality selection, format filtering, SponsorBlock integration, metadata embedding
- 🔄 **Auto-Update** — Checks GitHub for newer releases and updates silently
- 📊 **Concurrent Downloads** — Queue and manage multiple downloads simultaneously (capped at 4 on startup)
- 🖼️ **Thumbnail Embedding** — Automatic thumbnail download and embedding for videos and audio
- 🌐 **Browser Cookies** — Use saved cookies from Firefox, Chrome, Edge, or other browsers for age-restricted content
- 🛡️ **Anti-Bot Support** — Optional JavaScript runtime (Deno/Node.js) for handling YouTube anti-bot challenges

## Installation

### Windows (Recommended)

Download the latest installer from [Releases](https://github.com/vincentwetzel/MediaDownloader/releases):

1. Download `MediaDownloader-Setup-X.X.X.exe`
2. Run the installer
3. Launch from Start Menu or desktop shortcut

### From Source

Requires Python 3.10+ and PyInstaller.

```bash
# Clone the repo
git clone https://github.com/vincentwetzel/MediaDownloader.git
cd MediaDownloader

# Install dependencies
pip install -r requirements.txt

# Run from source
python main.pyw
```

## Usage

1. **Launch the app** from Start Menu or command line (`python main.pyw`)
2. **Enter a URL** in the "Start a Download" tab
3. **Configure options** (quality, format, SponsorBlock, etc.)
4. **Click Download** to start
5. **Monitor progress** in the "Active Downloads" tab
6. **Find completed files** in your configured output folder

## Configuration

All settings are saved to `settings.ini` and persist between sessions:

- **Output folder** — Where completed downloads are saved (required on first launch)
- **Temporary folder** — Where downloads are cached during progress (auto-created)
- **Quality/Format** — Video/audio codec and quality preferences
- **Metadata** — Embed titles, artists, and thumbnails
- **SponsorBlock** — Automatically skip sponsored segments, intros, outros
- **Browser Cookies** — Select a browser to use for authentication
- **yt-dlp Channel** — Choose stable (default) or nightly builds
- **Auto-Update** — Enable/disable startup checks for new versions (default: enabled)

See **Advanced Settings** tab in the app for full configuration.

## Building from Source (For Developers)

If you want to build your own Windows installer:

### Prerequisites

- Python 3.10+
- PyInstaller (in `requirements.txt`)
- NSIS (from https://nsis.sourceforge.io/Download)

### Build Steps

```bash
# 1. Download binaries for all platforms
.\download_binaries.ps1

# 2. Update version (optional)
# Edit core/version.py: __version__ = "X.X.X"

# 3. Build with PyInstaller
pyinstaller main.spec

# 4. Build NSIS installer
"C:\Program Files (x86)\NSIS\makensis.exe" MediaDownloader.nsi

# Output: MediaDownloader-Setup-X.X.X.exe
```

For detailed build and release instructions, see [UPDATE_AND_RELEASE.md](UPDATE_AND_RELEASE.md).

## Auto-Update System

The app automatically checks GitHub for newer releases on startup (can be disabled in Advanced Settings).

When a new version is available:
1. A dialog shows the release tag and changelog
2. Click "Yes" to download and install
3. The NSIS installer runs silently with elevated privileges
4. App restarts with the new version

## Architecture

```
MediaDownloader/
├── main.pyw                    # Entry point
├── core/                        # Core business logic
│   ├── download_manager.py     # Download queue and file handling
│   ├── yt_dlp_worker.py        # yt-dlp subprocess and progress parsing
│   ├── config_manager.py       # Settings persistence
│   ├── updater.py              # GitHub release checking and auto-update
│   ├── playlist_expander.py    # Playlist detection and expansion
│   ├── archive_manager.py      # Download history tracking
│   ├── logger_config.py        # Logging setup
│   └── version.py              # App version constant
├── ui/                          # PyQt6 user interface
│   ├── main_window.py          # Main application window
│   ├── tab_start.py            # Download initiation tab
│   ├── tab_active.py           # Active/completed downloads tab
│   ├── tab_advanced.py         # Advanced settings tab
│   └── widgets.py              # Custom widgets
├── utils/                       # Utilities
│   ├── cookies.py              # Browser cookie extraction
│   └── validators.py           # Input validation
├── tests/                       # Test scripts
│   └── auto_test.py            # Automated integration test
├── bin/                         # Bundled binaries (git-ignored)
│   ├── windows/                # Windows: yt-dlp.exe, ffmpeg.exe, ffprobe.exe
│   ├── linux/                  # Linux: yt-dlp, ffmpeg, ffprobe
│   └── macos/                  # macOS: yt-dlp_macos, ffmpeg, ffprobe
├── requirements.txt            # Python dependencies
└── main.spec                   # PyInstaller configuration
```

## Key Technologies

- **PyQt6** — Cross-platform GUI framework
- **yt-dlp** — Powerful media downloader
- **ffmpeg** — Video/audio processing
- **requests** — HTTP library for GitHub API checks
- **PyInstaller** — Python to Windows executable compiler
- **NSIS** — Nullsoft Scriptable Install System for Windows installers

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Commit your changes (`git commit -am 'Add feature'`)
4. Push to the branch (`git push origin feature/your-feature`)
5. Open a Pull Request

## Troubleshooting

### "Python is not installed"

This error only appears when running from source. If you downloaded the Windows installer, this means the compiled executable is corrupted. Try re-downloading from Releases.

### Update check fails

- Verify your internet connection
- Check that GitHub is accessible
- Try disabling auto-check in Advanced Settings and use "Check for App Update" button manually

### Downloads fail with "video unavailable"

- Try updating yt-dlp: Advanced Settings > "Update yt-dlp"
- Some sites may require browser cookies (Advanced Settings > select a browser)
- For age-restricted content, ensure you're logged in to your browser before launching the app

### Installer requires admin privileges

NSIS installers on Windows require elevated privileges to write to `Program Files`. If installation fails:
- Right-click the installer and select "Run as administrator"
- Or choose a different installation folder with write permissions

## License

This project is provided as-is. See LICENSE file for details.

## Support

For bugs, feature requests, or questions:
- Open an issue on GitHub: https://github.com/vincentwetzel/MediaDownloader/issues
- Check existing issues for solutions

## Changelog

See [CHANGELOG.md](CHANGELOG.md) or GitHub Releases for version history.

---

**Current Version:** 0.0.7  
**Last Updated:** February 9, 2026
