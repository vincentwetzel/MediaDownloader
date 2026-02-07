# -*- mode: python ; coding: utf-8 -*-

a = Analysis(
    ['main.pyw'],
    pathex=[],
    binaries=[],
    datas=[('bin', 'bin')],
    hiddenimports=[
        'core',
        'core.yt_dlp_worker',
        'core.app_updater',
        'core.config_manager',
        'core.download_manager',
        'core.file_ops_monitor',
        'core.logger_config',
        'core.playlist_expander',
        'core.version',
        'utils',
        'utils.cookies',
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