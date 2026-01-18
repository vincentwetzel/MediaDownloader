# -*- mode: python ; coding: utf-8 -*-
import shutil
import os

# Attempt to locate binaries from system PATH
yt_dlp_path = shutil.which('yt-dlp')
ffmpeg_path = shutil.which('ffmpeg')
ffprobe_path = shutil.which('ffprobe')

binaries = []

if yt_dlp_path:
    print(f"Found yt-dlp at: {yt_dlp_path}")
    binaries.append((yt_dlp_path, '.'))
else:
    print("WARNING: yt-dlp not found in PATH. It will not be bundled.")

if ffmpeg_path:
    print(f"Found ffmpeg at: {ffmpeg_path}")
    binaries.append((ffmpeg_path, '.'))
else:
    print("WARNING: ffmpeg not found in PATH. It will not be bundled.")

if ffprobe_path:
    print(f"Found ffprobe at: {ffprobe_path}")
    binaries.append((ffprobe_path, '.'))
else:
    print("WARNING: ffprobe not found in PATH. It will not be bundled.")

a = Analysis(
    ['main.pyw'],
    pathex=[],
    binaries=binaries,
    datas=[],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
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
