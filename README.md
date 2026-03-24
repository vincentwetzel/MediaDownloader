# MediaDownloader (C++ Port)

A lightweight, high-performance desktop application for downloading media (video and audio) from online platforms using **yt-dlp**.

**This is a C++ port of the original Python application.** It is designed to be a drop-in replacement, offering faster startup times, lower memory usage, and a native look and feel while maintaining full compatibility with existing user settings and download history.

## Features

- 🎬 **Download Video & Audio** — Support for YouTube, TikTok, Instagram, and 1000+ other sites via yt-dlp
- 🎵 **Audio Extraction** — Extract audio as MP3, M4A, opus, or other formats
- 📋 **Playlist Support** — Download entire playlists with configurable behavior
- 🖼️ **Gallery Support** — Download image galleries from supported sites (e.g., Instagram, Twitter) via `gallery-dl`
- 🎨 **Advanced Settings** — Quality selection, format filtering, SponsorBlock integration, metadata embedding
- 🔄 **Auto-Update** — Checks GitHub for newer releases and updates silently
- 📊 **Concurrent Downloads** — Queue and manage multiple downloads simultaneously
- 🖼️ **Thumbnail Embedding** — Automatic thumbnail download and embedding for videos and audio
- 🌐 **Browser Cookies** — Use saved cookies from Firefox, Chrome, Edge, or other browsers for age-restricted content
- 📂 **Smart Sorting** — Automatically organize downloads into subfolders based on uploader, playlist, date, or custom patterns

## Installation

### Windows (Recommended)

Download the latest installer from [Releases](https://github.com/vincentwetzel/MediaDownloader/releases):

1. Download `MediaDownloader-Setup-X.X.X.exe`
2. Run the installer
3. Launch from Start Menu or desktop shortcut

**Note for existing users:** The installer will automatically replace your existing Python version of MediaDownloader. Your settings and download history will be preserved.

### From Source

Requires CMake, a C++20 compatible compiler (MSVC recommended on Windows), and Qt 6.

```bash
# Clone the repo
git clone https://github.com/vincentwetzel/MediaDownloader.git
cd MediaDownloader

# Configure and build
cmake -B build
cmake --build build --config Release
```

## Usage

1. **Launch the app** (`MediaDownloader.exe`)
2. **Enter a URL** in the "Start Download" tab
3. **Configure options** (quality, format, SponsorBlock, etc.)
4. **Click Download** to start
5. **Monitor progress** in the "Active Downloads" tab
6. **Find completed files** in your configured output folder

## Configuration

All settings are saved to `settings.ini` and persist between sessions. The format is identical to the Python version, ensuring seamless migration.

- **Output folder** — Where completed downloads are saved
- **Temporary folder** — Where downloads are cached during progress
- **Quality/Format** — Video/audio codec and quality preferences
- **Metadata** — Embed titles, artists, and thumbnails
- **SponsorBlock** — Automatically skip sponsored segments
- **Browser Cookies** — Select a browser to use for authentication

## Architecture

The application is built using **C++20** and the **Qt 6** framework.

```
MediaDownloader/
├── CMakeLists.txt              # Build System Configuration
├── main.cpp                    # Application Entry Point
├── src/
│   ├── core/                   # Core Business Logic
│   │   ├── ConfigManager.h/cpp   # Settings persistence (INI)
│   │   ├── ArchiveManager.h/cpp  # Download history (SQLite)
│   │   ├── DownloadManager.h/cpp # Queue & Lifecycle Management
│   │   ├── YtDlpWorker.h/cpp     # QProcess Wrapper & Parsing
│   │   └── ...
│   ├── ui/                     # User Interface (Qt Widgets)
│   │   ├── MainWindow.h/cpp      # Main Window & Signal Hub
│   │   ├── StartTab.h/cpp        # Input Tab
│   │   ├── ActiveDownloadsTab.h/cpp # Progress Tab
│   │   └── ...
│   └── utils/                  # Helper Modules
│       └── StringUtils.h/cpp   # String/URL utilities
├── bin/                        # External Binaries
│   ├── yt-dlp.exe
│   ├── ffmpeg.exe
│   ├── ffprobe.exe
│   ├── gallery-dl.exe
│   ├── aria2c.exe
│   └── deno.exe
```

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Commit your changes (`git commit -am 'Add feature'`)
4. Push to the branch (`git push origin feature/your-feature`)
5. Open a Pull Request

## License

This project is provided as-is. See LICENSE file for details.
