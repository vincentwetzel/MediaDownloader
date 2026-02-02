# Minimal hook for PyQt6.QtGui to avoid complex analysis
# This bypasses the problematic hook that was causing hangs

from PyInstaller.utils.hooks import collect_submodules, get_module_file_attribute

hiddenimports = collect_submodules('PyQt6.QtGui')
