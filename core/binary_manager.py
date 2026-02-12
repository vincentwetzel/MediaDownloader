import os
import sys
import platform
import shutil
import logging

log = logging.getLogger(__name__)

# This dictionary maps a binary name to its relative path within the project
# for different operating systems.
_BINARY_PATHS = {
    "windows": {
        "yt-dlp": "bin/windows/yt-dlp.exe",
        "gallery-dl": "bin/windows/gallery-dl.exe",
        "ffmpeg": "bin/windows/ffmpeg-8.0.1-essentials_build/bin/ffmpeg.exe",
        "ffprobe": "bin/windows/ffmpeg-8.0.1-essentials_build/bin/ffprobe.exe",
        "deno": "bin/windows/deno.exe",
        "aria2c": "bin/windows/aria2-1.37.0-win-64bit-build1/aria2c.exe"
    },
    "linux": {
        "yt-dlp": "bin/linux/yt-dlp",
        "gallery-dl": "bin/linux/gallery-dl.bin",
        "ffmpeg": "bin/linux/ffmpeg",
        "ffprobe": "bin/linux/ffprobe",
        "deno": "bin/linux/deno",
        "aria2c": "bin/linux/aria2c"
    },
    "darwin": { # macOS
        "yt-dlp": "bin/macos/yt-dlp_macos",
        "gallery-dl": "bin/macos/gallery-dl",
        "ffmpeg": "bin/macos/ffmpeg",
        "ffprobe": "bin/macos/ffprobe",
        "deno": "bin/macos/deno",
        "aria2c": "bin/macos/aria2c"
    }
}

def _get_base_path():
    """
    Gets the base path for resources, accommodating both development and
    PyInstaller-bundled (onedir or onefile) environments.
    """
    if getattr(sys, 'frozen', False):
        # When bundled by PyInstaller, the location depends on the build type.
        if hasattr(sys, '_MEIPASS'):
            # This is a one-file build. Resources are in a temporary directory.
            return sys._MEIPASS
        else:
            # This is a one-dir build. Resources are in the executable's directory.
            return os.path.dirname(sys.executable)
    else:
        # Not frozen, so we're running from source.
        # The project root is one level up from the 'core' directory.
        return os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def _get_bundled_binary_path(name):
    """Gets the absolute path to a bundled binary, if it exists."""
    system = platform.system().lower()
    if system not in _BINARY_PATHS:
        return None

    relative_path = _BINARY_PATHS[system].get(name)
    if not relative_path:
        return None

    binary_path = os.path.normpath(os.path.abspath(os.path.join(_get_base_path(), relative_path)))
    
    if os.path.exists(binary_path):
        return binary_path
    
    return None

def get_bundled_binary_path(name):
    """Public wrapper to get the bundled binary path (or None)."""
    return _get_bundled_binary_path(name)

def get_system_binary_path(name):
    """Gets the absolute path to a binary found in the system PATH (or None)."""
    system_path = shutil.which(name)
    if not system_path:
        return None
    return os.path.normpath(os.path.abspath(system_path))

def get_ffmpeg_location(prefer_system=True):
    """
    Resolve a directory for yt-dlp's --ffmpeg-location.
    Prefers system ffmpeg+ffprobe if both exist in the same directory.
    Falls back to bundled binaries when system ones are missing.
    """
    system_ffmpeg = get_system_binary_path("ffmpeg")
    system_ffprobe = get_system_binary_path("ffprobe")

    if prefer_system and system_ffmpeg and system_ffprobe:
        system_dir = os.path.dirname(system_ffmpeg)
        if os.path.dirname(system_ffprobe) == system_dir:
            log.debug("Using system ffmpeg/ffprobe from: %s", system_dir)
            return system_dir
        log.warning("System ffmpeg/ffprobe found but in different directories. Ignoring system paths.")

    bundled_ffmpeg = _get_bundled_binary_path("ffmpeg")
    bundled_ffprobe = _get_bundled_binary_path("ffprobe")
    if bundled_ffmpeg and bundled_ffprobe:
        bundled_dir = os.path.dirname(bundled_ffmpeg)
        if os.path.dirname(bundled_ffprobe) != bundled_dir:
            log.warning("Bundled ffmpeg/ffprobe are in different directories. Using ffmpeg dir.")
        log.debug("Using bundled ffmpeg/ffprobe from: %s", bundled_dir)
        return bundled_dir

    # As a last resort, use whatever system ffmpeg exists
    if system_ffmpeg:
        system_dir = os.path.dirname(system_ffmpeg)
        log.warning("Only system ffmpeg found (ffprobe missing). Using: %s", system_dir)
        return system_dir

    return None

def get_binary_path(name):
    """
    Gets the absolute path to a named binary, preferring the bundled version.
    1. Checks for a bundled binary.
    2. If not found, falls back to the system's PATH.
    Returns the path if found, otherwise None.
    """
    # 1. Prioritize bundled binary
    bundled_path = _get_bundled_binary_path(name)
    if bundled_path:
        log.debug(f"Using bundled '{name}': {bundled_path}")
        return bundled_path

    # 2. Fall back to system PATH
    system_path = get_system_binary_path(name)
    if system_path:
        log.debug(f"Found '{name}' in system PATH as fallback: {system_path}")
        return system_path

    log.warning(f"Binary '{name}' not found in bundled binaries or system PATH.")
    return None
