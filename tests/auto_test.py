import sys
import logging
import os
import sys as _sys
import shutil
# Ensure project root is on sys.path so package imports like `ui.*` work
_sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import QTimer
from ui.main_window import MediaDownloaderApp
from core.logger_config import setup_logging

# Configure logging to console and file
setup_logging()

# Clean temp_downloads before test
temp_dir = os.path.join(os.path.dirname(__file__), "..", "temp_downloads")
if os.path.exists(temp_dir):
    for f in os.listdir(temp_dir):
        fpath = os.path.join(temp_dir, f)
        try:
            if os.path.isfile(fpath):
                os.remove(fpath)
                print(f"[TEST] Cleaned: {f}")
        except Exception as e:
            print(f"[TEST] Failed to clean {f}: {e}")

app = QApplication(sys.argv)
wnd = MediaDownloaderApp()
wnd.show()

# Reduce max_threads to 2 to force queuing
try:
    wnd.config_manager.set("General", "max_threads", "2")
except Exception:
    pass

urls = [
    "https://www.youtube.com/shorts/W8mQ0PyIOc4",
]

# Start downloads after a short delay so UI is ready
QTimer.singleShot(1000, lambda: wnd.start_downloads(urls, {}))

# Quit after 120 seconds to allow full download + embed + move
QTimer.singleShot(120000, app.quit)

sys.exit(app.exec())