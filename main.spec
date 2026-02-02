# -*- mode: python ; coding: utf-8 -*-
import shutil
import os
import platform

# --- Prepare binaries for bundling ---
system = platform.system()
project_root = os.path.dirname(os.path.abspath(__file__)) if '__file__' in locals() else os.getcwd()

# Create a temporary directory for binaries that might need renaming
temp_bin_dir = os.path.join(project_root, 'build', 'temp_bin')
os.makedirs(temp_bin_dir, exist_ok=True)

binaries_to_bundle = []

# --- yt-dlp binary handling ---
# We now bundle the specific yt-dlp binary from the 'bin' directory.
# For macOS, we rename 'yt-dlp_macos' to 'yt-dlp' to match the executable name
# our application's code expects in a bundled environment.
if system == "Windows":
    source_bin = os.path.join(project_root, "bin", "windows", "yt-dlp.exe")
    if os.path.exists(source_bin):
        print(f"Found yt-dlp for Windows: {source_bin}")
        binaries_to_bundle.append((source_bin, '.'))
    else:
        print(f"WARNING: yt-dlp for Windows not found at {source_bin}")

elif system == "Darwin":
    source_bin = os.path.join(project_root, "bin", "macos", "yt-dlp_macos")
    target_path = os.path.join(temp_bin_dir, "yt-dlp")
    if os.path.exists(source_bin):
        print(f"Found yt-dlp for macOS: {source_bin}. Staging as 'yt-dlp' for bundling.")
        shutil.copy(source_bin, target_path)
        binaries_to_bundle.append((target_path, '.'))
    else:
        print(f"WARNING: yt-dlp for macOS not found at {source_bin}")

else:  # Assuming Linux for others
    source_bin = os.path.join(project_root, "bin", "linux", "yt-dlp")
    if os.path.exists(source_bin):
        print(f"Found yt-dlp for Linux: {source_bin}")
        binaries_to_bundle.append((source_bin, '.'))
    else:
        print(f"WARNING: yt-dlp for Linux not found at {source_bin}")


# --- ffmpeg/ffprobe handling (now bundled) ---
if system == "Windows":
    ffmpeg_dir = os.path.join(project_root, "bin", "windows", "ffmpeg-8.0.1-essentials_build", "bin")
    ffmpeg_path = os.path.join(ffmpeg_dir, 'ffmpeg.exe')
    ffprobe_path = os.path.join(ffmpeg_dir, 'ffprobe.exe')

    if os.path.exists(ffmpeg_path):
        print(f"Found bundled ffmpeg: {ffmpeg_path}")
        binaries_to_bundle.append((ffmpeg_path, '.'))
    else:
        print(f"WARNING: Bundled ffmpeg.exe not found at {ffmpeg_path}")

    if os.path.exists(ffprobe_path):
        print(f"Found bundled ffprobe: {ffprobe_path}")
        binaries_to_bundle.append((ffprobe_path, '.'))
    else:
        print(f"WARNING: Bundled ffprobe.exe not found at {ffprobe_path}")
else:
    # Fallback to PATH for other systems (macOS/Linux)
    ffmpeg_path = shutil.which('ffmpeg')
    if ffmpeg_path:
        print(f"Found ffmpeg in PATH: {ffmpeg_path}")
        binaries_to_bundle.append((ffmpeg_path, '.'))
    else:
        print("WARNING: ffmpeg not found in PATH. It will not be bundled.")

    ffprobe_path = shutil.which('ffprobe')
    if ffprobe_path:
        print(f"Found ffprobe in PATH: {ffprobe_path}")
        binaries_to_bundle.append((ffprobe_path, '.'))
    else:
        print("WARNING: ffprobe not found in PATH. It will not be bundled.")


a = Analysis(
    ['main.pyw'],
    pathex=[],
    binaries=binaries_to_bundle,
    datas=[],
    hiddenimports=['PyQt6.QtCore', 'PyQt6.QtGui', 'PyQt6.QtWidgets'],
    hookspath=[os.path.join(project_root, 'hooks')],  # Use custom hooks directory
    hooksconfig={},
    runtime_hooks=[],
    excludes=['IPython', 'matplotlib', 'numpy'],  # Exclude heavy optional modules
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    [],
    [],
    name='MediaDownloader',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='MediaDownloader',
)