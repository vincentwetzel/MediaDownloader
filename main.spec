# -*- mode: python ; coding: utf-8 -*-
import os
from PyQt6.QtCore import QLibraryInfo

qt_plugins_path = QLibraryInfo.path(QLibraryInfo.LibraryPath.PluginsPath)
project_root = os.getcwd()

a = Analysis(
    ['main.pyw'],
    pathex=[project_root],
    binaries=[],
    datas=[
        (os.path.join(project_root, 'bin/windows/yt-dlp.exe'), 'bin/windows'),
        (os.path.join(project_root, 'bin/windows/deno.exe'), 'bin/windows'),
        (os.path.join(project_root, 'bin/windows/aria2-1.37.0-win-64bit-build1'), 'bin/windows/aria2-1.37.0-win-64bit-build1'),
        (os.path.join(project_root, 'bin/windows/ffmpeg-8.0.1-essentials_build'), 'bin/windows/ffmpeg-8.0.1-essentials_build'),
        (qt_plugins_path, 'PyQt6/Qt6/plugins'),
    ],
    hiddenimports=[
        'qdarktheme',
        'PIL'
    ],
    hookspath=['hooks'],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
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